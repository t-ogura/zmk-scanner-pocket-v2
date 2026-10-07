# Scanner Pocket v2 ハードウェアメモ（ファームウェアが参照する範囲）

正本は `SCANNER_POCKET_V2_HANDOVER.md`。ここでは firmware が依存するピンだけを、
**確定** と **要確認（PROVISIONAL）** に分けて写しています。要確認のものは完成 KiCad の
ネット名で照合するまで信用しないこと。

## MCU

FDK HY0020 = Nordic **nRF52832-CIAA**（WLCSP、build code G1。2026-10-08 に OpenOCD の `nrf5 info` で実機確認）、
512 KiB Flash / 64 KiB RAM → Zephyr の SoC は `nrf52832_ciaa`（QFAA と同じメモリ、compatible だけ異なる）。USB なし。ブートローダなし、SWD で `zephyr.hex` を直接書く。

## 確定

| 機能 | nRF52832 | 備考 |
|---|---|---|
| SWDCLK / SWDIO | 専用パッド（G4 / H4） | FFC 経由でプログラマへ |
| nRESET | P0.21（J4） | `&uicr { gpio-as-nreset; }` で有効化。リセット SW + FFC |
| VDD | D1 | 0.1 µF デカップリング |
| DCC ↔ DEC4 | M1 ↔ N1 | モジュール内で結線。**DCDC は実測後に有効化**（現状 LDO） |

Target 側 FFC（6 ピン）: 1 VCC / 2 SWD_CLK / 3 GND / 4 RST / 5 SWDIO / 6 VCC。
プログラマ側と 1↔6 反転接続になる前提だが、**実機でテスター確認するまで確定扱いしない**。

## 要確認（PROVISIONAL）

| 機能 | nRF52832 | DTS での扱い |
|---|---|---|
| LCD SCLK | P0.06 | `spi1_default` pinctrl。**実機確認済み** |
| LCD SI (MOSI) | P0.07 | 同上。**実機確認済み** |
| LCD SCS | P0.08 | `cs-gpios`、**Active HIGH**。**実機確認済み** |
| LCD EXTCOMIN | P0.12 | `extcomin-gpios`。**実機確認済み**。周波数は 10 Hz 暫定（データシート値に差し替え予定） |
| LCD DISP | P0.16 | `disp-en-gpios`。初期化後 High。**実機確認済み**（ピン探索ファームでも確認） |
| レバー SW 1/2/3 | P0.18 / P0.20 / P0.30 | `kscan0` direct GPIO、pull-up、active-low。**2026-10-08 実機確認済み（確定扱いに昇格）** |

## LCD（Sharp LS013B7DH05、144×168、1bit）

> **2026-10-08 実機確認済み**: SCLK P0.06 / SI P0.07 / SCS P0.08 / EXTCOMIN P0.12 / DISP P0.16 で
> Zephyr の `sharp,ls0xx` ドライバ（`nordic,nrf-spi`、1 MHz、CS Active HIGH、LSB first、
> CS 遅延 0）が**そのまま正常動作**する。上表の「要確認」は解消。
>
> **FPC の向きに注意。** 接点面が逆でも DISP・電源・一部の信号は絶縁面越しに薄く導通し、
> 「CLEAR だけ効く」「全画素黒を書くと不規則な縞」「駆動強度やタイミングで模様が変わる」
> 「指で押しても変わらない」という、電気的・プロトコル的な故障にそっくりな症状になる
> （`docs/lcd_stripes_2026-10-08.jpg`）。LCD が白のまま、または縞が出るときは、
> **信号やファームを疑う前に FPC の接点面の向きと挿入深さを確認すること。**


- `EXTMODE = VCC`（外部 VCOM 反転）→ firmware が EXTCOMIN を周期トグルする。Zephyr の
  `sharp,ls0xx` ドライバが `extcomin-gpios` + `extcomin-frequency` で面倒を見る
- DISP は GPIO。VDD 安定 → インタフェース初期化 → 初期クリア → DISP High
- 外付け: DISP–VSS 560 pF、VDDA–VSSA 1 µF、VDD–VSS 1 µF

## 電源

CR2032 → BAT+ → 電源 SW → VCC。プログラマ接続時は **電池 SW OFF、VPROG 3.3 V 給電** が標準運用
（現行プログラマはレベルシフタ無しの 3.3V 専用）。
