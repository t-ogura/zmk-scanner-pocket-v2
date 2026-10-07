# zmk-scanner-pocket-v2

FDK **HY0020**（Nordic nRF52832、512 KiB / 64 KiB）を直載せした Scanner Pocket v2 の ZMK / Zephyr
ファームウェア。MCU がモジュール直載せなので「MCU ボード + シールド」ではなく、
**Scanner Pocket v2 そのものを ZMK の custom board**（Hardware Model v2）として定義しています。

書き込みは UF2 ではなく **SWD**（`swd-programmer` の RP2040-Zero プローブ + OpenOCD）で `zephyr.hex` を直接書きます。

設計の正本は `SCANNER_POCKET_V2_HANDOVER.md`。firmware が参照するピンの要約は [docs/hardware.md](docs/hardware.md)。

## 構成

```
config/
├─ west.yml                      ZMK を 354cff9c（2026-01-17）に固定
├─ scanner_pocket_v2.conf        ZMK 固有設定はすべてここ（ZMK_BLE, KEYBOARD_NAME, SETTINGS_NVS）
├─ scanner_pocket_v2.keymap      レバー 3 接点 → A / B / C
└─ boards/t_ogura/scanner_pocket_v2/
   ├─ board.yml                  HWMv2 ボード定義（soc: nrf52832）
   ├─ Kconfig.scanner_pocket_v2  SOC_NRF52832_QFAA を選択
   ├─ Kconfig.defconfig          BT_CTLR のみ（純 Zephyr）
   ├─ scanner_pocket_v2_defconfig  MPU / FLASH、HEX 出力、UF2 なし（純 Zephyr）
   ├─ scanner_pocket_v2.dts      チップ・パーティション・kscan・(disabled) LCD
   ├─ scanner_pocket_v2-pinctrl.dtsi  LCD SPI ピン
   ├─ board.cmake                west flash → openocd（cmsis-dap, nrf52）
   └─ *.yaml / *.zmk.yml         メタデータ
app/phase_a_blink/               Phase A: ZMK 抜きの GPIO トグル（最初に SWD で書くもの）
scripts/flash.sh, flash.ps1      OpenOCD で program / verify / reset を 1 コマンド化
build.yaml                       CI 用ビルド行列
```

ボード定義は**純粋な Zephyr** に保っています（ZMK シンボルを一切含まない）。そうしないと ZMK 抜きの
`app/phase_a_blink` が「未定義シンボル」の Kconfig 警告で止まるためで、ZMK 固有の設定は `config/scanner_pocket_v2.conf` 側に集めてあります。

ボード定義を `config/boards/` に置いているのは、ZMK が `ZMK_CONFIG` を `BOARD_ROOT` に追加する
（`zmk/app/keymap-module/modules/modules.cmake`）ため、追加の `zephyr/module.yml` 無しで
発見されるからです。別リポジトリから共有したくなったら module 化すればよい。

## 判断した点

| 項目 | 判断 | 理由 |
|---|---|---|
| SoC | `nrf52832_qfaa` | HY0020 の 512K/64K に一致。QFAB は 256K/32K |
| ブートローダ | なし | SWD 直書きなので不要。アプリは 0x0 から、設定領域は末尾 24 KiB |
| nRESET | `gpio-as-nreset` | P0.21 をリセットに。リセット SW と FFC の両方が効く |
| DCDC | **無効（LDO）** | モジュール内 DCC/DEC4 結線は firmware だけで判断しない（資料 §24）。実測後に有効化 |
| USB | 一切なし | nRF52832 に USB ペリフェラル無し。`ZMK_USB` は設定しない |
| LCD ノード | DTS にあるが **disabled** | ネット名の KiCad 照合待ち。EXTCOMIN 周波数はデータシートから入れる |
| レバー入力 | direct GPIO、**PROVISIONAL** | P0.18/20/30 は設計値。KiCad で確認するまで仮 |
| コンソール | なし（必要時 RTT） | UART 未結線。`CONFIG_ZMK_RTT_LOGGING=y` で SWD 越しに読む |

## ビルド

```bash
cd zmk-scanner-pocket-v2
.venv/bin/west build -s zmk/app -b scanner_pocket_v2 -- \
  -DZMK_CONFIG="$PWD/config"
# → build/zephyr/zmk.hex
```

初回のみ: `python3 -m venv .venv && .venv/bin/pip install west && .venv/bin/west init -l config && .venv/bin/west update && .venv/bin/west zephyr-export && .venv/bin/pip install -r zephyr/scripts/requirements-base.txt`

Phase A の最小アプリ（ZMK 抜き。SWD と board 定義の疎通確認用）:

```bash
.venv/bin/west build -s app/phase_a_blink -d build-phase-a -b scanner_pocket_v2 -- \
  -DBOARD_ROOT="$PWD/config" -DDTS_ROOT="$PWD/zmk/app"
# → build-phase-a/zephyr/zephyr.hex   P0.16 を 1 Hz、P0.12 を 2 Hz でトグル
```

## 書き込み

```bash
scripts/flash.sh build/zephyr/zmk.hex            # Linux / WSL（usbipd で USB を attach）
.\scripts\flash.ps1 build\zephyr\zmk.hex         # Windows ネイティブ OpenOCD
```

初回は `--speed 500` 推奨。内部は `openocd -f interface/cmsis-dap.cfg -f target/nordic/nrf52.cfg -c "program <hex> verify reset exit"`。
失敗時は `build/zephyr/flash.log` が残ります。`west flash -r openocd` も board.cmake で使えます。

## 現状

| 日付 | 内容 |
|---|---|
| 2026-10-08 | リポジトリ作成。HWMv2 ボード定義、ZMK ビルド成功: **FLASH 178,616 B (35.7%) / RAM 41,844 B (63.9%)**。Phase A アプリ作成。**実機未接続**（プローブ・基板とも未検証） |

RAM は ZMK のキーボード用 BLE スタック込みで既に 64 KiB の 64%。Phase D で LVGL を載せる際は
ヒープを 8〜16 KiB に抑え、フレームバッファ（3 KiB × 枚数）を数えること。

## 既知の注意

- `west.yml` は ZMK を固定していますが、ZMK が import する Zephyr は `v4.1.0+zmk-fixes` **ブランチ**なので
  `west update` のたびに動きます（今回取得: `10ba6d0c` 2026-08-13）。再現性が要るなら Zephyr も SHA 固定にする
- ビルド時の `battery.c` の `#warning`（BATTERY ラベル非推奨）は電池センサ未定義のため。VDD 内部測定を実装するときに `zmk,battery` chosen を定義して解消
