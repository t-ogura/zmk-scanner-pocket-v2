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
app/phase_d_lcd/                 Phase D: ZMK/LVGL 抜きで LCD にテストパターン（display API 直叩き）
app/phase_d_pinfind/             診断: HY0020 の 12 GPIO を順に High にして DISP を目視で探す
app/phase_d_bitbang/             診断: SPI/表示ドライバを使わず GPIO で LS013B7DH05 を直接駆動
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
| SoC | `nrf52832_ciaa` | 実機で OpenOCD が `nRF52832-CIAA (G1)` と報告。512K/64K。QFAA との差は compatible のみ |
| ブートローダ | なし | SWD 直書きなので不要。アプリは 0x0 から、設定領域は末尾 24 KiB |
| nRESET | `gpio-as-nreset` | P0.21 をリセットに。リセット SW と FFC の両方が効く |
| DCDC | **無効（LDO）** | モジュール内 DCC/DEC4 結線は firmware だけで判断しない（資料 §24）。実測後に有効化 |
| USB | 一切なし | nRF52832 に USB ペリフェラル無し。`ZMK_USB` は設定しない |
| LCD ノード | **有効**、SPI は `nordic,nrf-spi` | nRF52832 は PAN 58 で SPIM が既定無効。**5 本とも実機で動作確認済み**。EXTCOMIN 10 Hz は暫定（データシート値に差し替え） |
| レバー入力 | direct GPIO | P0.18/20/30 を**実機確認済み**（3 接点とも BLE で A/B/C が入る） |
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

## APPROTECT の解除（出荷時ブートローダ入りの HY0020）

HY0020 は出荷時にブートローダが書かれ、**APPROTECT（デバッグアクセス保護）が有効**でした。
`init; reset halt` で `SWD DPIDR 0x2ba01477` は読めるのに `Could not find MEM-AP` と
`AP lock engaged` が出るのがその症状です。

OpenOCD 標準の `nrf52_recover` は**この個体では失敗します**（`ERASEALLSTATUS` が 1 のまま）。
`target/nordic/nrf52.cfg` が作る Cortex-M ターゲットの examination がロック中は失敗し、
その状態で発行した CTRL-AP ERASEALL が進まないためです。ターゲットを定義せず DAP だけで
CTRL-AP を叩く [scripts/nrf52_recover_daponly.cfg](scripts/nrf52_recover_daponly.cfg) なら
100 ms 以内に完了します（2026-10-08 実機確認）。

```powershell
openocd.exe -f interface/cmsis-dap.cfg -f scripts\nrf52_recover_daponly.cfg `
  -c "adapter speed 500" -c "init" -c "recover_daponly" -c "exit"
# APPROTECTSTATUS = 1 (after) が出たら、ターゲットを電源断（10 秒）→ 通常の init; reset halt
```

`target/nordic/nrf52.cfg` は**同時に読み込まない**こと。消去はフラッシュ全域と UICR を消し、
出荷時ブートローダは失われます（この設計では使わないので問題ない）。電源不足が原因ではなかった
ことも確認済み（消去中の VTREF min 3.02 V、3.0 V 未満のサンプル 0）。

### 電源を切ると再ロックされる（hardened APPROTECT）

解除後に電源を入れ直すと**再びロック**されました。この個体は新しいシリコンリビジョンで、
APPROTECT が電源投入時のデフォルトで有効になる仕様（hardened APPROTECT）です。
開いた状態を保つには **UICR.APPROTECT = 0x5A（HwDisabled）** が書かれていて、かつファームウェアが
起動時に `APPROTECT.DISABLE` を書く必要があります。後者は Zephyr の既定
（`CONFIG_NRF_APPROTECT_USE_UICR=y` → MDK が `DISABLE = UICR.APPROTECT` を実行）で満たされます。

したがって初回だけ、**解除 → 電源を切らずに → UICR に 0x5A → 書き込み** を一続きで行います:

```powershell
$env:OPENOCD = "C:\tools\xpack-openocd-0.12.0-7\bin\openocd.exe"
.\scripts\first_flash.ps1 build-phase-a\zephyr\zephyr.hex -Speed 500
```

（Linux/WSL: `scripts/first_flash.sh`）。以後は UICR が 0x5A のまま残るので `flash.ps1` で普通に書けます。
`flash write_image erase` はアプリのセクタしか消さないため UICR は保たれます。再び ERASEALL を
したときだけ、もう一度 `first_flash` が必要です。

## 現状

| 日付 | 内容 |
|---|---|
| 2026-10-08 | リポジトリ作成。HWMv2 ボード定義、ZMK ビルド成功: **FLASH 178,616 B (35.7%) / RAM 41,844 B (63.9%)**。Phase A アプリ作成 |
| 2026-10-08 | 実機: プローブから SWD 疎通 OK。HY0020 は出荷時 APPROTECT 有効（hardened、電源断で再ロック）→ `first_flash.ps1` で解除＋UICR 0x5A＋Phase A 書き込み・verify 成功。**資料 §26 の 7 項目達成**。チップは nRF52832-CIAA G1。Phase A が実機で動作（RTT で `tick` カウント確認、`scripts/rtt.cfg`）。ZMK 本体も通常の `flash.ps1` で書込・verify OK（UICR 0x5A 後は `program` のリセットでも再ロックされない）。**Phase B 達成**: BLE 広告が見える（プローブ給電下）。**Phase C 達成**: ペアリング後にレバー 3 接点で A/B/C 入力。名前を `Scanner Pocket` に変更。LCD ノード有効化＋`app/phase_d_lcd` 作成 |
| 2026-10-08 | **Phase D 達成**: `app/phase_d_lcd` のテストカードが `ls0xx` ドライバ経由で正常表示。それまでの「白のまま」「不規則な縞」は **LCD の FPC の接点面が逆**だったのが原因。基板 rev1 はコネクタ向きが設計と逆で、**逆挿しで開発続行、次版で修正**（docs/hardware.md 参照）。第二マイルストーンの 3 要素（BLE・レバー・LCD）が個別に動作 |
| 2026-10-08 | ZMK に LCD を統合（内蔵ステータス画面、LVGL 1bpp、プール 8 KiB、VDB 100%）: **FLASH 315,824 B (63.2%) / RAM 62,324 B (95.1%)**。表示系の増分は 20,480 B。実機確認待ち（第二マイルストーン） |

RAM: ZMK＋キーボード用 BLE スタックで 41.8 KiB、LCD（LVGL 1bpp）を足すと 62.3 KiB（95%）。
残り 3 KiB。削減余地: VDB を部分描画（`LV_Z_VDB_SIZE` 25% ＋ `LV_Z_AREA_X_ALIGNMENT_WIDTH=144` で
ls0xx の全幅要件を満たす）と ZMK 既定のプール 4 KiB で 8 KiB 前後。Phase E（observer 専用）では
キーボード用 BLE スタックが不要になり、v1 の測定では約 32 KiB 戻る。

## 既知の注意

- `west.yml` は ZMK を固定していますが、ZMK が import する Zephyr は `v4.1.0+zmk-fixes` **ブランチ**なので
  `west update` のたびに動きます（今回取得: `10ba6d0c` 2026-08-13）。再現性が要るなら Zephyr も SHA 固定にする
- ビルド時の `battery.c` の `#warning`（BATTERY ラベル非推奨）は電池センサ未定義のため。VDD 内部測定を実装するときに `zmk,battery` chosen を定義して解消
