# デバッグ状態
[![DebugBuild](https://github.com/TakeuchiIori/YoRigine_Engine/actions/workflows/Debug.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine_Engine/actions/workflows/Debug.yml)

[![ReleaseBuild](https://github.com/TakeuchiIori/YoRigine_Engine/actions/workflows/Release.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine_Engine/actions/workflows/Release.yml)
# CodeFactor
[![CodeFactor](https://www.codefactor.io/repository/github/TakeuchiIori/YoRigine_Engine/badge)](https://www.codefactor.io/repository/github/TakeuchiIori/YoRigine_Engine)

# ビルド方法
1. ダウンロードしたら **`premake.bat`** を起動します。
2. **`.sln`** (ソリューションファイル) が生成されるので、**Visual Studio** で開きます。
3. ビルドを開始します。

# ※ビルドできない場合

ビルドツール **Premake** の実行には、プロジェクトの配置場所について以下の制約があります。

* ダウンロードしたフォルダを含む**ディレクトリパス**には、**日本語**や**全角文字**など、**英数字と一部の記号（ASCII文字）以外**の文字を使用しないでください。
* Premakeなど一部のビルドツールが、非ASCII文字を含むパスを正しく処理できないため、**実行に失敗します**。

| 状態 | パスの例 |
| :--- | :--- |
| **✅ OK** | `C:\Users\username\Documents\project_name` |
| **❌ NG** | `C:\**ユーザー**\**ドキュメント**\**プロジェクト名**` |
