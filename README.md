# Homebrew Launcher for Nintendo Switch

Wii U [Homebrew Launcher](https://github.com/dimok789/homebrew_launcher) の見た目を、Switch 公式 [nx-hbmenu](https://github.com/switchbrew/nx-hbmenu)（[nyaa-ko/nx-hbmenu](https://github.com/nyaa-ko/nx-hbmenu)）ベースに移植した NRO です。

機能（起動・netloader・フォルダ・NRO スキャン）は nx-hbmenu のまま、UI だけ Dimok の HBL に合わせています。

## 見た目

- 水色グラデーション背景 `(79,153,239) → (59,159,223)`
- パーティクル（HBL と同じ 500）
- 1ページ 4 本の横長バナーボタン（実アセット）
- **NRO アイコンは 256×256 のまま正方形で表示**（256×96 に潰さない）
- 選択ハイライト＋スムーズな拡大
- ページ送りは線形 35px ではなく **exponential ease-out**
- Load / Back の起動確認ウィンドウ
- フッター `Homebrew Launcher v1.5-nx by Dimok`
- ループ BGM（`romfs/bgMusic.ogg`）

nx-hbmenu 由来のバージョン文字・パス・ボタンヒント・ステータスバーは描画せず、HBL の配置だけが残ります。

## 操作

| ボタン | 動作 |
| --- | --- |
| 上下 | 同じページ内の項目 |
| 左右 / L R / ZL ZR | ページ送り（4件単位） |
| A | 起動確認 → Load |
| B | 戻る / ダイアログを閉じる |
| Y | NetLoader（nxlink） |
| X | スター |
| - | テーマメニュー |
| + | 終了 |

タッチ: バナーをタップで選択、もう一度で起動確認。左右スワイプでページ送り。

## BGM

起動時に `romfs/bgMusic.ogg` をデコードして audout でループ再生します。失敗してもランチャー自体は動きます。

音量は `common/hbl_ui.h` の `HBL_BGM_VOLUME`（0.0〜1.0、標準 0.50）。

## アニメーション速度のいじり方

**編集するファイルは [`common/hbl_ui.h`](common/hbl_ui.h) の先頭だけです。**

```c
#define HBL_PAGE_LERP        0.14f  /* ページ送り。小さいほどゆっくり滑らか */
#define HBL_PAGE_SNAP_PX     0.8f   /* これより近い位置はスナップ */
#define HBL_SELECT_LERP      0.20f  /* 選択バナーの拡大速度 */
#define HBL_SELECT_SCALE     0.96f  /* 選択中の倍率 */
#define HBL_IDLE_SCALE       0.90f  /* 非選択の倍率 */
#define HBL_LAUNCH_FADE      0.10f  /* 起動ダイアログのフェードイン */
#define HBL_PARTICLE_SPEED   1.00f  /* パーティクル速度倍率 */
#define HBL_BGM_VOLUME       0.50f  /* BGM 音量 */
```

目安（すべて 1 フレームあたり、約 60fps）:

| 定数 | 遅め | 標準 | 速め |
| --- | --- | --- | --- |
| `HBL_PAGE_LERP` | 0.08 | 0.14 | 0.22〜0.32 |
| `HBL_SELECT_LERP` | 0.10 | 0.20 | 0.30 |
| `HBL_LAUNCH_FADE` | 0.06 | 0.10 | 0.18 |
| `HBL_PARTICLE_SPEED` | 0.50 | 1.00 | 2.00 |

`HBL_SELECT_SCALE` を `HBL_IDLE_SCALE` と同じ `0.90f` にすると、選択時の拡大アニメは消えます。

実装は [`common/hbl_ui.c`](common/hbl_ui.c) の `tickPageMotion()` / `tickSelectScale()`。線形 `+= 35` はやめて、毎フレーム `current += (target - current) * LERP` の ease-out です。

## SD 配置

ビルドした `homebrew_launcher.nro` を次のどちらかへ:

- `sd:/hbmenu.nro` （Atmosphere が読む標準メニューとして差し替え）
- `sd:/switch/homebrew_launcher/homebrew_launcher.nro`

Homebrew 本体は従来どおり `sd:/switch/` 以下の `.nro`。

GitHub Actions の成果物には `hbmenu.nro` のコピーも入っています。

## ビルド

devkitPro が必要です。

```
sudo dkp-pacman -S switch-dev switch-freetype switch-libconfig switch-libjpeg-turbo switch-physfs
export DEVKITPRO=/opt/devkitpro
make nx
```

成果物: `homebrew_launcher.nro`

PC プレビュー（SDL2）: `make pc`

OGG デコードは [stb_vorbis](https://github.com/nothings/stb)（public domain）を `third_party/stb_vorbis.c` に同梱しているので、追加の audio ライブラリは不要です。

## ライセンス

- UI アセット・レイアウトは dimok789/homebrew_launcher（GPL-3.0）
- ランチャー本体は switchbrew/nx-hbmenu（ISC）ベース
- stb_vorbis は public domain / MIT-0

結合バイナリは GPL-3.0 として配布してください。

## クレジット

- Dimok ほか Wii U Homebrew Launcher 作者
- switchbrew / fincs / yellows8 ほか nx-hbmenu 作者
