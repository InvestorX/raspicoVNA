# raspicoVNA
Raspberry Pi Pico 2 リアルタイムFFTスペクトラムアナライザ

## 概要
このプロジェクトは、Raspberry Pi Pico 2を使用してアナログ信号のリアルタイムFFT解析を行い、結果をVGA出力でバーグラフ表示するシステムです。

## 特徴
- **128kHzサンプリング**: 高速なADCサンプリングでリアルタイム処理を実現。
- **64タップFIRローパスフィルタ**: 50kHzカットオフで不要な高周波を除去。
- **リアルタイム256点FFT**: 高速フーリエ変換で周波数成分を解析。
- **dBm単位の振幅スペクトル表示**: 結果を電力単位（dBm）で可視化。
- **VGA出力**: 320×240解像度のモノクロバーグラフを表示（VGA→HDMI変換対応）。
- **DMA二重バッファリング**: サンプリング欠落を防ぐ設計。

## ハードウェア要件
- **Raspberry Pi Pico 2**
- **抵抗ラダー回路**: VGA信号生成用。
- **ADC入力回路**: サンプリング信号の入力用。
- **VGA→HDMI変換アダプタ**: HDMIディスプレイ対応。

## ソフトウェア要件
- **Pico SDK**: Raspberry Pi Pico用の公式開発環境。
- **KissFFT**: 高速フーリエ変換ライブラリ。
- **CMake**: ビルドシステム。

## ディレクトリ構造
```
raspicoVNA/
├── src/
│   ├── main.c                # メインプログラム
│   ├── adc_dma.c             # ADCとDMA管理
│   ├── fir_filter.c          # FIRフィルタ処理
│   ├── vga_output.c          # VGA出力管理
│   └── spectrum_display.c    # スペクトラム表示処理
├── include/
│   ├── config.h              # 設定ファイル
│   ├── adc_dma.h             # ADCとDMAヘッダ
│   ├── fir_filter.h          # FIRフィルタヘッダ
│   ├── vga_output.h          # VGA出力ヘッダ
│   └── spectrum_display.h    # スペクトラム表示ヘッダ
├── pio/
│   └── vga.pio               # PIOプログラム（VGA信号生成）
├── lib/
│   └── kissfft/              # FFTライブラリ
├── docs/
│   ├── hardware_setup.md     # ハードウェア設定ドキュメント
│   ├── calibration.md        # 校正方法
│   └── troubleshooting.md    # トラブルシューティング
└── CMakeLists.txt            # ビルド設定
```

## ビルドと実行
### 1. 必要な依存関係をインストール
- Pico SDKをインストールします。
- KissFFTライブラリを`lib/`ディレクトリに配置します。

### 2. リポジトリをクローン
```bash
git clone https://github.com/InvestorX/raspicoVNA.git
cd raspicoVNA
```

### 3. ビルド設定
```bash
mkdir build
cd build
cmake ..
make
```

### 4. 実行
ビルドが完了したら、生成されたバイナリをRaspberry Pi Picoに書き込みます。

## 使用方法
1. Raspberry Pi Picoをセットアップし、信号入力を接続します。
2. VGA出力をモニターに接続します（必要に応じてVGA→HDMI変換アダプタを使用）。
3. 電源を入れると、リアルタイムでスペクトラムが表示されます。

## 開発環境
- **エディタ**: VSCode（推奨）
- **デバッグ**: PicoprobeまたはSWDデバッガ
- **OS**: Windows / macOS / Linux

## 貢献方法
1. リポジトリをフォークします。
2. 新しいブランチを作成します。
3. 機能追加やバグ修正を行います。
4. プルリクエストを送信します。

## ライセンス
このプロジェクトは「Sushi Ware」ライセンスの下で提供されます。  
「もしもこのソフトウェアが役に立つと思ったなら、寿司をおごってください！」という精神に基づいています。

### 寿司ライセンスの内容
```
/*
 * Sushi Ware License
 *
 * If you like this software and feel it’s helpful,
 * please buy me sushi sometime. 🍣
 */
```