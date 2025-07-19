# raspicoVNA
Raspberry Pi Pico 2 リアルタイムFFTスペクトラムアナライザ

## 概要
このプロジェクトは、Raspberry Pi Pico 2を使用してアナログ信号のリアルタイムFFT解析を行い、結果をVGA出力でバーグラフ表示するシステムです。

特徴として以下の機能を備えています：
- アナログ信号を128kHzでサンプリング
- FIRローパスフィルタ（64タップ）によるノイズ除去
- 256点の高速フーリエ変換（FFT）による周波数解析
- VGA出力（320×240解像度、モノクロバーグラフ）でスペクトラム表示
- DMAによる二重バッファリングでリアルタイム処理を実現
- dBm単位でのスペクトラム描画

なお、本プロジェクトのコードは以下のリポジトリを参考にしています：
- [ADCとFFTの実装例 - awulff-pico-playground](https://github.com/AlexFWulff/awulff-pico-playground/tree/e0c98d544ad0cf7972edaf5215ae165e835f29eb/adc_fft)

## 特徴
- **リアルタイムスペクトラム解析**: 高速なADCサンプリングとFFT解析を組み合わせ、リアルタイムで周波数成分を表示します。
- **ハードウェア最適化**: DMA二重バッファリングとPIOを活用して効率的なデータ処理を実現。
- **簡易表示**: dBm単位のバーグラフをVGA出力で直接描画。

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
│   ├── fft_vga_display_complete.c # メインプログラム
├── include/
│   ├── vga.pio.h # VGA出力用PIOプログラムヘッダ
├── lib/
│   └── kissfft/ # FFTライブラリ
├── docs/
│   ├── hardware_setup.md # ハードウェア設定ドキュメント
│   ├── calibration.md # 校正方法
│   └── troubleshooting.md # トラブルシューティング
└── CMakeLists.txt # ビルド設定
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

## License
### "THE SUSHI-WARE LICENSE"

InvestorX wrote this file.

As long as you retain this notice you can do whatever you want
with this stuff. If we meet some day, and you think this stuff
is worth it, you can buy me a **sushi 🍣** in return.

(This license is based on ["THE BEER-WARE LICENSE" (Revision 42)].
 Thanks a lot, Poul-Henning Kamp ;)

​["THE BEER-WARE LICENSE" (Revision 42)]: https://people.freebsd.org/~phk/
