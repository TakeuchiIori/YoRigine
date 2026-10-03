# ゴルディン (Goldin)

[![DebugBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml)
[![ReleaseBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml)

3Dフィールドを舞台に、敵を倒しながら進むアクションゲームです。
コンセプトは **「操作感の良いアクションゲーム」**。自作エンジン **YoRigine**（DirectX 12 / C++20）で制作しています。

<!-- TODO: ゲーム画面のスクリーンショット or GIF をここに貼る
![Goldin](Docs/images/goldin.gif)
-->

| | |
| :--- | :--- |
| ジャンル | 3Dアクションゲーム |
| エンジン | 自作エンジン（[YoRigine_Engine](https://github.com/TakeuchiIori/YoRigine_Engine)） |
| 開発環境 | Visual Studio 2022 / MSVC v143 / C++20 / Premake5 |
| 開発期間 | 開発継続中（15ヵ月〜） |

# 構成

エンジン本体は別リポジトリ（[YoRigine_Engine](https://github.com/TakeuchiIori/YoRigine_Engine)）で、
このリポジトリの `Engine/` に **Git サブモジュール**として取り込んでいます。

| 場所 | 内容 |
| :--- | :--- |
| `Engine/` | エンジン（YEngine / YMath / Externals）。サブモジュール |
| `YGame/` | ゲーム本体 |
| `YMain/` | 起動用 EXE |
| `Resources/` | リソース |
| `Tools/` | Premake、ビルド（`Build.bat`）、配布用パッケージ作成（`Package.bat`）、git フック（`githooks/`） |

# セットアップ（初回のみ）

<details>
<summary>手順を開く</summary>

1. **サブモジュールごと**クローンします。

   ```bash
   git clone --recurse-submodules https://github.com/TakeuchiIori/YoRigine.git
   ```

2. クローンしたフォルダの **`Setup.bat`** をダブルクリックします。
   * Engine サブモジュールの取得確認
   * git フックの有効化
   * Premake による **`YoRigine.sln`** の生成
3. 生成された **`YoRigine.sln`** を **Visual Studio** で開き、ビルドします。

> ⚠ GitHub の **「Download ZIP」では Engine が取得されません**（`Engine/` が空になります）。
> 必ず `git clone --recurse-submodules` を使ってください。
> 先に普通に `git clone` した場合も、`Setup.bat` が Engine を取得します。

</details>

# 日常の運用

<details>
<summary>手順を開く</summary>

* **`git pull` やブランチ切り替えのあとは、Premake が自動で実行されます**（git フック）。
  Visual Studio で「プロジェクトが変更されました」と出たら **再読み込み**してください。
* ファイルを追加したメンバーの変更を取り込めば、自動で `.sln` / `.vcxproj` が更新されます。
* `.sln` と `.vcxproj` は生成物なので、Git では管理していません。

## Engine を更新するとき

Engine は別リポジトリです。Engine の変更は次の順で反映します。

1. `Engine/` の中で `git switch master` → 変更 → コミット → **push**
2. このリポジトリのルートで `Engine`（ポインタ）をコミット → push

順番を逆にすると、他のメンバーが Engine を取得できなくなります。

</details>

# ※ビルドできない場合

<details>
<summary>対処を開く</summary>

* `Engine/` が空 → `git submodule update --init --recursive` を実行してください。
* `.sln` が無い／古い → `Tools\premake.bat` を実行してください。
* ビルドツール **Premake** の実行には、プロジェクトの配置場所について以下の制約があります。

  * ダウンロードしたフォルダを含む**ディレクトリパス**には、**日本語**や**全角文字**など、**英数字と一部の記号（ASCII文字）以外**の文字を使用しないでください。
  * Premakeなど一部のビルドツールが、非ASCII文字を含むパスを正しく処理できないため、**実行に失敗します**。

| 状態 | パスの例 |
| :--- | :--- |
| **✅ OK** | `C:\Users\username\Documents\project_name` |
| **❌ NG** | `C:\**ユーザー**\**ドキュメント**\**プロジェクト名**` |

</details>
