# zmk-scanner-pocket-v2

**Scanner Pocket v2** — FDK **HY0020**（Nordic nRF52832、512 KiB Flash / 64 KiB RAM）を直載せした、
Sharp Memory LCD 付きの小型 Prospector スキャナ。近くの Prospector 対応キーボードの
レイヤー・修飾キー・電池・WPM を受信して表示します。CR2032 で動きます。

ファームウェアは ZMK / Zephyr。HY0020 には USB が無いので、書き込みは
**SWD**（専用プローブ [swd-ffc6-programmer](https://github.com/t-ogura/swd-ffc6-programmer) ＋ OpenOCD）で `zephyr.hex` を直接書きます。UF2 のドラッグ&ドロップはありません。

## 必要なもの

| もの | 備考 |
|---|---|
| Scanner Pocket v2 基板 | rev1 は LCD コネクタの向きが設計と逆。**FPC を逆向きに挿す**（[docs/hardware.md](docs/hardware.md)） |
| [swd-ffc6-programmer](https://github.com/t-ogura/swd-ffc6-programmer) | RP2040-Zero ベースの CMSIS-DAP プローブ。6 ピン FFC で接続、3.3 V も供給 |
| OpenOCD 0.12 以降 | [xPack OpenOCD](https://github.com/xpack-dev-tools/openocd-xpack/releases) を展開するだけ。Windows でも WSL でも可 |
| ビルド環境 | West ＋ Zephyr SDK（下記）。ビルド済み hex を使うなら不要 |

## ファームウェアを書く

### 初回（出荷時 HY0020、または ERASEALL 後）

HY0020 は出荷時に **APPROTECT（デバッグ保護）が有効**で、しかも新リビジョンは電源を入れ直すたびに
再ロックされます。初回は「解除 → 電源を切らずに → UICR に解除フラグ → 書き込み」を一続きで行う
スクリプトを使います。

```powershell
# PowerShell（Windows）。プローブを USB 接続、Scanner Pocket の電池スイッチは OFF
$env:OPENOCD = "C:\tools\xpack-openocd-0.12.0-7\bin\openocd.exe"
powershell -ExecutionPolicy Bypass -File .\scripts\first_flash.ps1 build\zephyr\zmk.hex -Speed 500
```

```bash
# Linux / WSL（USB は usbipd で attach、OpenOCD は ~/.local/opt/openocd 想定）
scripts/first_flash.sh build/zephyr/zmk.hex --speed 500
```

成功すると `APPROTECTSTATUS = 1 (after)` → `Cortex-M4 r0p1 processor detected` → `wrote ...` → `verified ...` と進みます。
出荷時ブートローダは消えます（この設計では使いません）。

### 2 回目以降

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash.ps1 build\zephyr\zmk.hex -Speed 500
```

```bash
scripts/flash.sh build/zephyr/zmk.hex --speed 500
```

`** Verified OK **` が出れば完了。失敗時は hex と同じ場所の `flash.log` に OpenOCD の出力が残ります。
`AP lock engaged` が出たら初回手順に戻ってください（UICR が消えた＝再ロック）。

### 動いているか見る

- 画面: 起動後「Scanner Pocket」→ `Scanning...` → キーボードを受信すると名前・レイヤー・電池が出る
- レバー: 押し込みでキーボード一覧 ⇄ メイン、一覧では上下で選択（3 秒無操作でメインに戻る）
- ログ: UART は無いので **RTT**。OpenOCD を `-f scripts/rtt.cfg -c init -c "reset run" -c "sleep 500" -c rtt_go` で起動し、`localhost:9090` を TeraTerm（TCP/IP）等で開く。ログ付きビルドは `-DEXTRA_CONF_FILE=docs/rtt_logging.conf`

## ビルド

```bash
python3 -m venv .venv && .venv/bin/pip install west
.venv/bin/west init -l config && .venv/bin/west update && .venv/bin/west zephyr-export
.venv/bin/pip install -r zephyr/scripts/requirements-base.txt

.venv/bin/west build -s zmk/app -b scanner_pocket_v2 -- \
  -DSHIELD=scanner_pocket_v2 -DZMK_CONFIG="$PWD/config"
# → build/zephyr/zmk.hex
```

- `scanner_pocket_v2`（board）: この基板の定義。`config/boards/t_ogura/scanner_pocket_v2/`
- `scanner_pocket_v2`（shield）: Prospector モジュール側。observer の Kconfig と Pocket UI（`pocket_display.c`）を載せる
- GitHub Actions（`.github/workflows/build.yaml`）でも同じものがビルドされます

Zephyr SDK 0.16.5 / ZMK は `config/west.yml` で固定（2026-01-17 の `354cff9c`）。

## 構成

```
config/
├─ west.yml                      ZMK と prospector-zmk-module の取り込み
├─ scanner_pocket_v2.conf        observer 専用、LVGL 1bpp 部分描画、CR2032 電池、ログ無し
├─ scanner_pocket_v2.keymap      キーマップは &none（キーボードではない）
└─ boards/t_ogura/scanner_pocket_v2/   HWMv2 ボード定義（nrf52832_ciaa、LCD、レバー、電池、SWD 用 board.cmake）
scripts/
├─ flash.ps1 / flash.sh          通常の書き込み
├─ first_flash.ps1 / .sh         初回: APPROTECT 解除 + UICR + 書き込み
├─ nrf52_recover_daponly.cfg     解除の本体（DAP のみで CTRL-AP ERASEALL。標準の nrf52_recover はこの個体で失敗する）
└─ rtt.cfg                       RTT コンソール
app/
├─ phase_a_blink/                素の Zephyr: GPIO トグル（最初に SWD で書くもの）
├─ phase_d_lcd/                  素の Zephyr: LCD テストパターン
├─ phase_d_pinfind/              診断: GPIO を順に High にして DISP を探す
└─ phase_d_bitbang/              診断: GPIO 直叩きで LCD を駆動
docs/
├─ hardware.md                   ピン配置（確定/要確認）、rev1 の既知の不具合
├─ observer_trial*.conf, rtt_logging.conf   追加 conf のサンプル
└─ lcd_stripes_2026-10-08.jpg    FPC 逆挿しのときの見え方
```

## メモリ

| 構成 | FLASH | RAM（64 KiB） |
|---|---|---|
| 現行（observer ＋ 横向き UI ＋ 電池） | 200 KB (40%) | **48.5 KB (74%)** |
| ZMK キーボード役を残した場合 | — | 5 KB 超過で不可 |

## ハードウェアの判断（詳細は docs/hardware.md）

| 項目 | 判断 |
|---|---|
| SoC | `nrf52832_ciaa`（OpenOCD が実機で報告。512K/64K） |
| ブートローダ | 無し。アプリは 0x0 から、設定領域は末尾 24 KiB |
| nRESET | P0.21 を `gpio-as-nreset` |
| DCDC | 無効（LDO）。実測後に検討 |
| USB | 無し |
| LCD | ストック `ls0xx` ではなく v1 の回転ドライバ（DISP/EXTCOMIN 対応を追加）で横向き 168×144。SPI は `nordic,nrf-spi`（PAN 58） |
| レバー | P0.18/20/30 を gpio-keys で UI 操作。ZMK 用ダミー kscan は P0.28 |
| 電池 | SAADC の VDD 入力を直接測定（`prospector,battery-nrf-vdd`）、CR2032 曲線 |
| EXTCOMIN | 10 Hz **暫定**（データシート値に差し替え予定） |

## 残タスク

- 電池放電曲線の校正（3.0 V を割ってから）
- スキャンのデューティ比と省電力（現在 100% duty）
- EXTCOMIN 周波数のデータシート確認
- 2025-07 以前の 25 バイト形式で広告する古いキーボードへの対応（焼き直し推奨）、複数台の実機確認
- 基板 rev1 の LCD コネクタ向き修正

## 経緯（2026-10-08）

1 日で、プローブ単体試験 → APPROTECT 解除 → Phase A（GPIO）→ BLE → レバー → LCD → observer 受信 → 横向き → 電池表示まで到達。
詰まった箇所と教訓は [docs/hardware.md](docs/hardware.md) に残してあります（LCD の FPC の向き、hardened APPROTECT、
`SHIELD_SCANNER_POCKET` を v2 から立ててはいけない理由）。詳しい作業ログは git log を参照。
