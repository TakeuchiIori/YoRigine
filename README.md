# デバッグ状態
[![DebugBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml)
[![ReleaseBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml)

# 構成

エンジン本体は別リポジトリ（[YoRigine_Engine](https://github.com/TakeuchiIori/YoRigine_Engine)）で、
このリポジトリの `Engine/` に **Git サブモジュール**として取り込んでいます。

| 場所 | 内容 |
| :--- | :--- |
| `Engine/` | エンジン（YEngine / YMath / Externals）。サブモジュール |
| `YGame/` | ゲーム本体 |
| `YMain/` | 起動用 EXE |
| `Resources/` | リソース |

# セットアップ（初回のみ）

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

# 日常の運用

* **`git pull` やブランチ切り替えのあとは、Premake が自動で実行されます**（git フック）。
  Visual Studio で「プロジェクトが変更されました」と出たら **再読み込み**してください。
* ファイルを追加したメンバーの変更を取り込めば、自動で `.sln` / `.vcxproj` が更新されます。
* `.sln` と `.vcxproj` は生成物なので、Git では管理していません。

## Engine を更新するとき

Engine は別リポジトリです。Engine の変更は次の順で反映します。

1. `Engine/` の中で `git switch master` → 変更 → コミット → **push**
2. このリポジトリのルートで `Engine`（ポインタ）をコミット → push

順番を逆にすると、他のメンバーが Engine を取得できなくなります。

# ※ビルドできない場合

* `Engine/` が空 → `git submodule update --init --recursive` を実行してください。
* `.sln` が無い／古い → `Tools\premake.bat` を実行してください。
* ビルドツール **Premake** の実行には、プロジェクトの配置場所について以下の制約があります。

  * ダウンロードしたフォルダを含む**ディレクトリパス**には、**日本語**や**全角文字**など、**英数字と一部の記号（ASCII文字）以外**の文字を使用しないでください。
  * Premakeなど一部のビルドツールが、非ASCII文字を含むパスを正しく処理できないため、**実行に失敗します**。

| 状態 | パスの例 |
| :--- | :--- |
| **✅ OK** | `C:\Users\username\Documents\project_name` |
| **❌ NG** | `C:\**ユーザー**\**ドキュメント**\**プロジェクト名**` |
