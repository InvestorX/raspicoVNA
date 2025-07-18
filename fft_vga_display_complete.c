/**
 * @file fft_vga_display_complete.c
 * @brief Raspberry Pi Pico 2 で 128 kHz サンプリング＋FIR ローパス＋FFT → VGA（320×240×1bit）で
 *        バーグラフ表示し、市販の VGA→HDMI 変換アダプタで出力する完全サンプル
 */

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "kiss_fft.h"
#include <math.h>

//==============================================================================
// VGA 出力定義
//==============================================================================
// HSYNC, VSYNC, RGB 各1bit をまとめて出力する PIO プログラムを外部生成したヘッダ
// 事前に pico pioasm で vga.pio をコンパイルし、vga.pio.h を生成しておくこと
#include "vga.pio.h"

#define VGA_PIO         pio0
#define VGA_SM          0
#define VGA_HSYNC_PIN   2    // HSYNC
#define VGA_VSYNC_PIN   3    // VSYNC
#define VGA_RED_PIN     4    // RED (1bit)
#define VGA_GREEN_PIN   5    // GREEN (1bit)
#define VGA_BLUE_PIN    6    // BLUE (1bit)

#define FRAME_WIDTH     320
#define FRAME_HEIGHT    240

//==============================================================================
// FFT/ADC/FIR 定義
//==============================================================================
#define SAMPLE_NUM      256       // FFT 点数
#define FIR_TAP_NUM     64        // FIR タップ数
#define ADC_PIN         26        // ADC0 → GPIO26
#define SAMPLING_FREQ   128000    // 128 kHz
#define CUT_OFF_FREQ    50000.0f  // 50 kHz カットオフ

#define VREF            3.3f      // ADC 参照電圧
#define IMPEDANCE       50.0f     // 負荷インピーダンス Ω
#define MIN_DBM         -100.0f   // dBm 描画下限
#define MAX_DBM         0.0f      // dBm 描画上限

// ADC 二重バッファ
static uint16_t adc_buf0[SAMPLE_NUM];
static uint16_t adc_buf1[SAMPLE_NUM];
static volatile bool buf0_ready = false;
static volatile bool buf1_ready = false;
static int dma_chan0, dma_chan1;

// FIR フィルタ
static float fir_coeffs[FIR_TAP_NUM];
static float fir_state[FIR_TAP_NUM];
static int   fir_idx = 0;

// 窓関数バッファ
static float window_buf[SAMPLE_NUM];

// FFT バッファ
static kiss_fft_cfg fft_cfg;
static kiss_fft_cpx fft_in[SAMPLE_NUM];
static kiss_fft_cpx fft_out[SAMPLE_NUM];

// VGA フレームバッファ (1bit×FRAME_WIDTH×FRAME_HEIGHT/8)
static uint8_t framebuf[FRAME_HEIGHT][FRAME_WIDTH/8];

//==============================================================================
// 関数プロトタイプ
//==============================================================================
static void init_hanning_window(void);
static void init_fir_filter(void);
static float apply_fir(float x);
void dma_handler(void);
static void init_adc_dma_double(void);
static void init_vga_output(void);
static void convert_to_dBm(const float *mags, float *dBms);
static void draw_frame_dbm(const float *dBms);
static void process_fft_buffer(uint16_t *buf);

//==============================================================================
// 関数実装
//==============================================================================

/**
 * @brief ハニング窓を生成する
 */
static void init_hanning_window(void) {
    for (int i = 0; i < SAMPLE_NUM; i++) {
        window_buf[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (SAMPLE_NUM - 1)));
    }
}

/**
 * @brief FIR ローパスフィルタ係数を Hamming 窓付き sinc で設計・正規化
 */
static void init_fir_filter(void) {
    float sum = 0.0f;
    float fc = CUT_OFF_FREQ / SAMPLING_FREQ;  // 正規化カットオフ
    int M = FIR_TAP_NUM - 1;
    for (int n = 0; n <= M; n++) {
        float m = n - (float)M / 2.0f;
        float h = (fabsf(m) < 1e-6f)
            ? 2.0f * fc
            : sinf(2.0f * M_PI * fc * m) / (M_PI * m);
        float w = 0.54f - 0.46f * cosf(2.0f * M_PI * n / M);
        fir_coeffs[n] = h * w;
        sum += fir_coeffs[n];
        fir_state[n] = 0.0f;
    }
    for (int i = 0; i <= M; i++) {
        fir_coeffs[i] /= sum;
    }
    fir_idx = 0;
}

/**
 * @brief 単一サンプルに FIR フィルタ処理を適用する
 * @param x 入力サンプル (0.0–1.0 正規化振幅)
 * @return フィルタ出力 (0.0–1.0)
 */
static float apply_fir(float x) {
    fir_state[fir_idx] = x;
    float y = 0.0f;
    int idx = fir_idx;
    for (int i = 0; i < FIR_TAP_NUM; i++) {
        y += fir_coeffs[i] * fir_state[idx];
        if (--idx < 0) idx = FIR_TAP_NUM - 1;
    }
    if (++fir_idx >= FIR_TAP_NUM) fir_idx = 0;
    return y;
}

/**
 * @brief DMA IRQ ハンドラ (チャネル0/1 完了をフラグ化)
 */
void dma_handler(void) {
    uint32_t status = dma_hw->ints0;
    if (status & (1u << dma_chan0)) {
        dma_hw->ints0 = 1u << dma_chan0;
        buf0_ready = true;
    }
    if (status & (1u << dma_chan1)) {
        dma_hw->ints0 = 1u << dma_chan1;
        buf1_ready = true;
    }
}

/**
 * @brief ADC を 128 kHz で二重 DMA バッファリング開始
 */
static void init_adc_dma_double(void) {
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_PIN - 26);
    // チャネル0
    dma_chan0 = dma_claim_unused_channel(true);
    dma_channel_config cfg0 = dma_channel_get_default_config(dma_chan0);
    channel_config_set_transfer_data_size(&cfg0, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg0, false);
    channel_config_set_write_increment(&cfg0, true);
    channel_config_set_dreq(&cfg0, DREQ_ADC);
    channel_config_set_chain_to(&cfg0, dma_chan1);
    dma_channel_configure(dma_chan0, &cfg0,
                          adc_buf0, &adc_hw->fifo, SAMPLE_NUM, false);
    // チャネル1
    dma_chan1 = dma_claim_unused_channel(true);
    dma_channel_config cfg1 = dma_channel_get_default_config(dma_chan1);
    channel_config_set_transfer_data_size(&cfg1, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg1, false);
    channel_config_set_write_increment(&cfg1, true);
    channel_config_set_dreq(&cfg1, DREQ_ADC);
    channel_config_set_chain_to(&cfg1, dma_chan0);
    dma_channel_configure(dma_chan1, &cfg1,
                          adc_buf1, &adc_hw->fifo, SAMPLE_NUM, false);
    // IRQ 有効化
    dma_channel_set_irq0_enabled(dma_chan0, true);
    dma_channel_set_irq0_enabled(dma_chan1, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    // FIFO & ADC 開始
    adc_fifo_setup(true, true, 1, false, false);
    adc_run(true);
    dma_channel_start(dma_chan0);
}

/**
 * @brief PIO＋DMA 連携で VGA（320×240×1bit）出力を初期化
 */
static void init_vga_output(void) {
    // HSYNC/VSYNC/PIXEL 出力ピン設定
    gpio_set_function(VGA_HSYNC_PIN, GPIO_FUNC_PIO0);
    gpio_set_function(VGA_VSYNC_PIN, GPIO_FUNC_PIO0);
    for (int p = VGA_RED_PIN; p <= VGA_BLUE_PIN; p++) {
        gpio_set_function(p, GPIO_FUNC_PIO0);
    }
    // PIO プログラムをロード
    uint offset = pio_add_program(VGA_PIO, &vga_program);
    // SM コンフィグ取得・設定
    pio_sm_config c = vga_program_get_default_config(offset);
    // ピン配置
    sm_config_set_sideset_pins(&c, VGA_HSYNC_PIN);     // sideset[0]=HSYNC, sideset[1]=VSYNC
    sm_config_set_out_pins(&c, VGA_RED_PIN, 3);        // out pins: RGB
    // 1bit/pixel、FIFO→OUT、pin方向
    sm_config_set_out_shift(&c, true, false, 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    // ピクセルクロック設定 (≈6 MHz for 320×240@60Hz)
    float sys_clk = clock_get_hz(clk_sys);
    float target_hz = 25.175e6f / 4.0f; // 6.29 MHz
    sm_config_set_clkdiv(&c, sys_clk / target_hz);
    // SM 初期化&開始
    pio_sm_init(VGA_PIO, VGA_SM, offset, &c);
    pio_sm_set_enabled(VGA_PIO, VGA_SM, true);
}

/**
 * @brief 振幅スペクトルを dBm に変換
 * @param mags   入力振幅スペクトル（正規化0–1）
 * @param dBms   出力 dBm（長さ SAMPLE_NUM/2）
 */
static void convert_to_dBm(const float *mags, float *dBms) {
    for (int i = 0; i < SAMPLE_NUM/2; i++) {
        float v_peak = mags[i] * VREF;
        float v_rms  = v_peak / M_SQRT2;
        float p_mw   = (v_rms * v_rms) / IMPEDANCE * 1000.0f;
        dBms[i]      = 10.0f * log10f(fmaxf(p_mw, 1e-12f));
    }
}

/**
 * @brief dBm スペクトルをフレームバッファに 1bit バーグラフ描画
 * @param dBms 入力 dBm 配列
 */
static void draw_frame_dbm(const float *dBms) {
    memset(framebuf, 0, sizeof(framebuf));
    int bins = SAMPLE_NUM/2;
    int col_w = FRAME_WIDTH / bins;
    float range = MAX_DBM - MIN_DBM;

    for (int i = 0; i < bins; i++) {
        float norm = (dBms[i] - MIN_DBM) / range;
        norm = norm < 0 ? 0 : norm > 1 ? 1 : norm;
        int h = (int)(norm * FRAME_HEIGHT);
        for (int y = FRAME_HEIGHT-1; y >= FRAME_HEIGHT-h; y--) {
            for (int x = i*col_w; x < (i+1)*col_w; x++) {
                framebuf[y][x/8] |= 1 << (7 - (x % 8));
            }
        }
    }
    // DMA で PIO TX FIFO にフレームバッファ転送
    static int dma_vga;
    static bool inited = false;
    if (!inited) {
        dma_vga = dma_claim_unused_channel(true);
        dma_channel_config dc = dma_channel_get_default_config(dma_vga);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_8);
        channel_config_set_read_increment(&dc, true);
        channel_config_set_write_increment(&dc, false);
        channel_config_set_dreq(&dc, pio_get_dreq(VGA_PIO, VGA_SM, true));
        dma_channel_configure(dma_vga, &dc,
                              &VGA_PIO->txf[VGA_SM],  // 書き込み先
                              framebuf,               // 読み出し元
                              FRAME_HEIGHT*(FRAME_WIDTH/8),
                              true);
        inited = true;
    } else {
        dma_channel_set_read_addr(dma_vga, framebuf, true);
    }
}

/**
 * @brief ADC バッファを選んで FIR→窓→FFT→表示 までを実行
 * @param buf ADC サンプリングバッファ
 */
static void process_fft_buffer(uint16_t *buf) {
    // FFT 用マグニチュード算出
    float mags[SAMPLE_NUM/2];
    for (int i = 0; i < SAMPLE_NUM; i++) {
        float raw = buf[i] / 4095.0f;
        float f = apply_fir(raw) * window_buf[i];
        fft_in[i].r = f;
        fft_in[i].i = 0.0f;
    }
    kiss_fft(fft_cfg, fft_in, fft_out);
    for (int i = 0; i < SAMPLE_NUM/2; i++) {
        mags[i] = sqrtf(fft_out[i].r*fft_out[i].r + fft_out[i].i*fft_out[i].i);
    }
    // dBm 変換＋描画
    static float dBms[SAMPLE_NUM/2];
    convert_to_dBm(mags, dBms);
    draw_frame_dbm(dBms);
}

/**
 * @brief メインループ
 */
int main(void) {
    stdio_init_all();
    init_hanning_window();
    init_fir_filter();
    fft_cfg = kiss_fft_alloc(SAMPLE_NUM, 0, NULL, NULL);

    init_adc_dma_double();
    init_vga_output();

    while (true) {
        if (buf0_ready) {
            buf0_ready = false;
            process_fft_buffer(adc_buf0);
        }
        if (buf1_ready) {
            buf1_ready = false;
            process_fft_buffer(adc_buf1);
        }
        tight_loop_contents();
    }

    //  通常到達しないがリソース解放
    kiss_fft_free(fft_cfg);
    return 0;
}