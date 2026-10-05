# Homebrew Launcher for Nintendo Switch

Wii U [Homebrew Launcher](https://github.com/dimok789/homebrew_launcher) の見た目を、Switch 公式 [nx-hbmenu](https://github.com/switchbrew/nx-hbmenu)（[nyaa-ko/nx-hbmenu](https://github.com/nyaa-ko/nx-hbmenu)）ベースに移植した NRO です。

機能（起動・netloader・フォルダ・NRO スキャン）は nx-hbmenu のまま、UI だけ Dimok の HBL に合わせています。

## 見た目

- 水色グラデーション背景 `(79,153,239) → (59,159,223)`
- パーティクル（HBL と同じ 500）
- 1ページ 4 本の横長バナーボタン（`homebrewButton.png` 実アセット）
- 選択ハイライト（`homebrewButtonSelected.png`）
- ページ左右矢印
- Load / Back の起動確認ウィンドウ（`launchMenuBox.png`）
- フッター `Homebrew Launcher v1.5-nx by Dimok`

## 操作

| ボタン | 動作 |
| --- | --- |
| 上下 | 同じページ内の項目 |
| 左右 / L R / ZL ZR | ページ送り（4件単位） |
| A | 起動確認 → Load |
| B | 戻る / ダイアログを閉じる |
| Y | NetLoader（nxlink） |
| X | スター |
| - | テーマメニュー（残置） |
| + | 終了 |

## SD 配置

ビルドした `homebrew_launcher.nro` を次のどちらかへ:

- `sd:/hbmenu.nro` （Atmosphere が読む標準メニューとして差し替え）
- `sd:/switch/homebrew_launcher/homebrew_launcher.nro`

Homebrew 本体は従来どおり `sd:/switch/` 以下の `.nro`。

## ビルド

devkitPro が必要です。

```
sudo dkp-pacman -S switch-dev switch-freetype switch-libconfig switch-libjpeg-turbo switch-physfs
export DEVKITPRO=/opt/devkitpro
make nx
```

成果物: `homebrew_launcher.nro`（ディレクトリ名が TARGET）

PC プレビュー（SDL2）: `make pc`

## ライセンス

- UI アセット・レイアウトは dimok789/homebrew_launcher（GPL-3.0）
- ランチャー本体は switchbrew/nx-hbmenu（ISC）ベース

結合バイナリは GPL-3.0 として配布してください。

## クレジット

- Dimok ほか Wii U Homebrew Launcher 作者
- switchbrew / fincs / yellows8 ほか nx-hbmenu 作者
