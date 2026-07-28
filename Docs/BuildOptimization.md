# ビルド高速化の記録（2026-07-28）

YoRigine（Premake5 + VS2022 / MSVC v145 / C++20 / x64）のビルド時間を短縮した作業のまとめ。
「なぜやったか → 遅い原因 → 解決方法 → なぜその手段を選んだか → 結果」を記録する。

---

## 1. なぜビルドを速くしようと思ったか

- **フルビルド／リビルドが遅く**、ちょっとした確認のたびに待ち時間が発生していた。
- 特に **リビルド時に、自分で書いていない外部ライブラリまで毎回コンパイルされる**のが無駄に感じた。
  外部ライブラリのコードは基本的に自分で編集しないので、リビルドのたびに作り直す必要がないはず、という直感。
- 開発の反復速度（編集→ビルド→確認のサイクル）を上げることが目的。

---

## 2. ビルドが遅い原因

調査（プロパティ確認＋実測）の結果、遅さの正体は主に3つだった。

### 原因A：プリコンパイルヘッダ（PCH）が未使用
- `YEngine` は **283ファイル**、`YGame` は 76ファイル。
- PCH が無いため、**各 .cpp が毎回**  `<d3d12.h>` `<Windows.h>` `<wrl.h>` `nlohmann/json.hpp`
  `imgui.h` などの**巨大ヘッダを個別にパース**していた。
- `json.hpp` や DirectX/Windows ヘッダは単体で非常に重く、283回パースし直すのが最大のボトルネック。

### 原因B：`/ZI`（エディット＆コンティニュー用デバッグ情報）
- Debug が `/ZI` でコンパイルされており、`/Zi` より**デバッグ情報の生成が重い**。
- Edit & Continue（実行中コード書き換え）は使っていなかった。

### 原因C：DirectXTex を毎回ソースからビルドしていた（＋破壊的な罠）
- `DirectXTex` は externalproject として**ソースからビルド**される設定だった。
  21ファイル（`DirectXTexConvert.cpp` 5246行、`BC6HBC7.cpp` 3630行など巨大）＋ `fxc` による
  シェーダ生成7本を含み、外部の中で**圧倒的に重い**（ImGui は Rebuild 6.2秒で無罪）。
- さらに致命的な罠：DirectXTex の vcxproj は Clean 時（`ATGDeleteShaders`）に
  **git コミット済みの生成シェーダ `Shaders/Compiled/*.inc` を削除**し、`fxc` で再生成しようとする。
  VS Developer 環境外だと `fxc` が見つからず **9009 エラーで失敗**し、`.inc` が消えたまま残る。
  → この影響で **フルソリューションの Rebuild は実質的に実行不可能**だった。

> なお assimp / curl は元々ビルド済み `.lib` をリンクしているだけで、コンパイル対象ではない。
> ソースからビルドされる外部は **ImGui と DirectXTex の2つだけ**だった。

---

## 3. 解決方法（実施した施策）

| # | 施策 | 対象 | 効果 |
|---|---|---|---|
| ① | **PCH 導入** | YEngine / YGame | 重い外部・システムヘッダのパースを1回に集約 |
| ② | **PCH に YMath コアも追加** | YEngine / YGame | 全TUが使う安定ヘッダ（Vector/Matrix/Quaternion/MathFunc）を前計算 |
| ③ | **`/ZI` → `/Zi`**（`editandcontinue "Off"`） | Debug / Develop | デバッグ情報生成を軽量化 |
| ④ | **DirectXTex を事前ビルド版 `.lib` 直リンクに変更** | ソリューション全体 | ビルドグラフから除外。Rebuildでも一切コンパイルされない |

### 補足：PCH の設計方針
- PCH（`YEngine/pch.h` `YGame/pch.h`）には **「変更頻度が低く・多数の .cpp が使い・パースが重い」**
  ヘッダだけを入れる：STL / Windows / d3d12 / wrl / `<json.hpp>` /（Debug のみ）imgui / YMath コア。
- **自作ヘッダ（Logger.h 等）は入れない**。入れると、それを編集するたびに PCH 再生成 →
  プロジェクト全体が再コンパイルされ、PCH の目的が逆効果になるため。
- `forceincludes { "pch.h" }` で全 .cpp の先頭に自動注入するので、**既存ソースは無改修**。

### 副作用の修正（1件）
- PCH で `Windows.h` が全ファイルに入った結果、`ParticleCurve.h` の `bool near = ...` が
  `Windows.h` の空レガシーマクロ `near` と衝突（`C2513`）。→ 変数を `isNear` に改名して解決。
- PCH 内でのグローバル `#undef near/far/small` は、`rpcndr.h` の `#define small char` を壊し
  MIDL 生成ヘッダ（d3d12shader.h / dinput.h）をコンパイル不能にするため **不採用**。
  「コード側を直す」のが正解だった。

---

## 4. なぜこの実装方法を選んだか（選定理由）

### PCH を選んだ理由（vs 他のコンパイル高速化）
- **Unity（Jumbo）ビルド**：フルリビルドは最速化できるが、`static`／匿名名前空間の衝突リスクがあり、
  PCH 導入後は上乗せ幅も縮小。リスク対効果で見送り。
- **YEngine を複数の小さい StaticLib に分割**：並列ビルド・部分再リンクに有効だが、大規模リファクタが必要。
- **PCH**：`forceincludes` で**既存ソース無改修**、低リスク、かつ真のボトルネック（ヘッダ再パース）に
  ど真ん中で効く。→ **最小の変更で最大の効果**が得られるため採用。

### `/ZI`→`/Zi` を選んだ理由
- Edit & Continue を使っていないので、失うものがなく純粋にコンパイル/リンクが軽くなる。

### FASTLINK を「採用しなかった」理由
- リンク高速化候補の `/DEBUG:FASTLINK` は、本プロジェクトの toolset **v145 では廃止済み**で
  `LNK4315` 警告が出て無視される。Debug リンクは既に `/INCREMENTAL` が有効なので追加対応も不要。

### DirectXTex を「事前ビルド .lib 化」した理由（複数案からの選定）
検討した3案：

- **案A：運用で回避**（`Rebuild Solution` をやめ、自分のプロジェクトだけ Rebuild）
  → 設定変更ゼロだが、外部をビルドグラフから外せず、うっかり全体Rebuildすると再発。
- **案B：DirectXTex を事前ビルド .lib 化**（← 採用）
- **案C：ImGui も含めて両方 .lib 化**
  → 徹底できるが ImGui は 6.2秒で効果が小さく、変更範囲だけ増える。

**案B を選んだ決め手：**
1. **遅さの主犯は DirectXTex** に特定できていた（ImGui は軽微）。
2. **リポジトリの既存流儀に一致**：assimp / curl は既にビルド済み `.lib` を commit してリンクしている
   （assimp は Debug 82MB を commit 済み）。DirectXTex を同じ扱いにするのは自然で一貫性がある。
3. **CI が現在存在しない**ため、`.lib` 差し替えによる破綻リスクが無い。
4. **破壊的なシェーダ削除の罠が消える**：ビルドグラフから外れるので `.inc` を触らなくなり、
   実行不可能だったフルRebuildが安全に使えるようになる。
5. ユーザーの当初の直感（外部は毎回ビルドしたくない）を、**正しい形で実現**できる。

---

## 5. ビルド時間の変化（実測 / Debug）

### YEngine 単体フルコンパイル（同一コマンドで計測）

| 状態 | 時間 | 対ベースライン |
|---|---:|---:|
| ベースライン（PCH無し・/ZI・DirectXTexソースビルド） | **104 秒** | — |
| ＋PCH（外部/STL） | 40 秒 | −62% |
| ＋PCH に YMath 追加 ＋ `/Zi` | **27.6 秒** | **−73%** |

### フルソリューション Rebuild（DirectXTex .lib 化後）

| 構成 | Before | After |
|---|---|---|
| **Debug 全体 Rebuild** | **9009 で実行不可**（DirectXTexがシェーダ破壊） | **50.9 秒 / EXIT=0** |
| **Release 全体 Rebuild** | 同上 | **96.6 秒 / EXIT=0** |
| Rebuild時の DirectXTex コンパイル | 毎回フル（21ファイル＋fxcシェーダ7本） | **ゼロ**（.lib をリンクするだけ） |
| シェーダ `.inc` 破壊の罠 | あり | **消滅** |

参考：ImGui 単体 Rebuild は 6.2 秒（高速なので手を付けていない）。

---

## 変更したファイル

- `premake5.lua`：PCH 設定（YEngine/YGame）、`editandcontinue "Off"`、
  DirectXTex externalproject 削除＋vendored lib の libdirs 追加、`dependson` 調整
- `YEngine/pch.h` `YEngine/pch.cpp`（新規）
- `YGame/pch.h` `YGame/pch.cpp`（新規）
- `YEngine/Generators/Particle/ParticleCurve.h`：`near` → `isNear`
- `Externals/DirectXTex/lib/{Debug,Release}/DirectXTex.lib`（新規・vendored）
- `.gitignore`：DirectXTex lib フォルダの除外解除ルールを追加

---

## メンテナンス手順（重要）

### YMath を頻繁に編集する時期が来たら
`YEngine/pch.h`・`YGame/pch.h` の YMath include 行を外す。
（PCH に自作ヘッダが入っていると、それを編集するたびに全再コンパイルになるため。）

### DirectXTex を更新した時だけ（滅多に無い）
1. `Externals/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj` を Debug / Release でビルド
   （元 vcxproj・`Shaders/`・`.inc` は再生成用にディスクへ残置してある）。
2. 生成された `DirectXTex.lib` を `Externals/DirectXTex/lib/{Debug,Release}/` へコピーして差し替え。

### Edit & Continue を使いたくなったら
`premake5.lua` の `editandcontinue "Off"` を削除して premake 再生成。

---

## 今後さらに速くする余地（未着手）

- **増分ビルドの本丸＝ホットヘッダの依存削減**：`DirectXCommon.h`（32ファイルが include）等を
  前方宣言 / pimpl で切ると、そのヘッダを編集したときの再コンパイル範囲が激減する。
  PCH では解決できない領域で、日々の「1ファイル編集→再ビルド」を軽くする施策。
- Unity ビルド / YEngine のライブラリ分割は効果大だが工数・リスクも大きいため保留。

---

## 用語解説

このドキュメントに出てくる用語を、読み方つきで説明する。

### デバッグ情報フォーマット（`/ZI` と `/Zi` の違い）★重要

コンパイラ（`cl.exe`）に渡す「デバッグ情報をどう作るか」のオプション。**大文字/小文字で意味が違う。**

| 表記 | 読み方 | 意味 | 速さ |
|---|---|---|---|
| `/ZI` | ゼット**大文字**アイ | **Edit & Continue 対応**のデバッグ情報。実行中にコードを書き換えて反映できるが、生成が**重い** | 遅い |
| `/Zi` | ゼット**小文字**アイ | 通常の PDB（デバッグ情報）を生成。E&C は使えないが**軽い** | 速い |
| `/Z7` | ゼットセブン | デバッグ情報を .obj に直接埋める（PDB を使わない）古い方式 | — |

> 今回は E&C を使っていなかったので **`/ZI` → `/Zi`** に変更して軽量化した。
> premake では `editandcontinue "Off"` を指定すると `/Zi` になる。

### リンカのデバッグオプション（`/DEBUG` 系）

リンカ（`link.exe`）に渡す、PDB の作り方の指定。

| 表記 | 読み方 | 意味 |
|---|---|---|
| `/DEBUG` = `/DEBUG:FULL` | デバッグ（フル） | 全 .obj のデバッグ情報を1つの PDB に統合。完全だがリンクが遅い |
| `/DEBUG:FASTLINK` | ファストリンク | PDB に統合せず各 .obj を参照するだけ。速いが制約あり。**toolset v145 では廃止** |
| `/INCREMENTAL` | インクリメンタル（増分） | 変更部分だけを差分リンクしてリンク時間を短縮。本プロジェクトの Debug で有効 |

### ビルド用語

- **PCH（ピーシーエイチ / プリコンパイルヘッダ）**：Pre-Compiled Header。よく使うヘッダを
  **一度だけコンパイルして使い回す**仕組み。関連オプション：
  - `/Yc`（ワイシー）… PCH を**生成**する（`pch.cpp` に付く）
  - `/Yu`（ワイユー）… PCH を**使用**する（他の全 .cpp に付く）
  - `/FI`（エフアイ / force include）… 指定ヘッダを各 .cpp の先頭に**自動挿入**する。
    premake の `forceincludes` がこれ。おかげで既存ソースを書き換えずに PCH を適用できる。
- **TU（ティーユー / 翻訳単位）**：Translation Unit。1つの .cpp ＋ そこから include される全ヘッダを
  まとめた「コンパイルの1単位」。「283ファイル」＝おおよそ 283 TU。
- **Rebuild / Build / Clean（リビルド / ビルド / クリーン）**：
  - *Build* … 変更があった所だけコンパイルする**増分ビルド**（速い）
  - *Clean* … 生成物（.obj/.lib 等）を全削除
  - *Rebuild* … Clean してから全部ビルド（＝**フルビルド**、遅い）
- **Unity ビルド / Jumbo ビルド（ユニティ / ジャンボ）**：複数の .cpp を1つに束ねてコンパイルし
  高速化する手法。ゲームエンジンの Unity とは無関係。
- **前方宣言（ぜんぽうせんげん / forward declaration）**：`class Foo;` だけ書いて実体の
  include を避け、ヘッダ依存を減らすテクニック。
- **pimpl（ピンプル）**：Pointer to IMPLementation。実装を .cpp 側に隠してヘッダの依存を切る設計。

### ライブラリ / リンク用語

- **`.lib`（リブ / ライブラリ）**：コンパイル済みコードの塊。
  - *static lib（スタティックリブ / 静的ライブラリ）*：リンク時に実行ファイルへ埋め込まれる。
  - *import lib（インポートリブ）*：DLL を使うための入口情報だけを持つ小さな .lib。
- **DLL（ディーエルエル）**：Dynamic Link Library。実行時に読み込まれる共有ライブラリ。
  本プロジェクトの YGame は Debug で DLL、Release で static lib。
- **vendoring（ベンダリング）**：外部ライブラリのビルド済み成果物（.lib 等）を自分のリポジトリに
  取り込んで持つこと。今回 DirectXTex.lib をこれにした（assimp/curl も同様）。
- **externalproject（エクスターナルプロジェクト）**：premake で外部の .vcxproj を参照する仕組み。
  これを外すと、その外部はビルドされなくなる。
- **staticruntime `/MT` `/MTd`（エムティー / エムティーディー）**：C ランタイムを静的リンクする設定。
  `/MTd` は Debug 版。リンクする .lib 同士でこれが食い違うとエラーになる。

### ツール / 生成物

- **fxc（エフエックスシー）**：HLSL シェーダをコンパイルする DirectX のツール（`fxc.exe`）。
  DirectXTex はビルド時にこれでシェーダを `.inc` に変換していた。
- **`.inc`（インク / インクルードファイル）**：fxc がシェーダをコンパイルして生成する C++ ヘッダ。
  DirectXTex では**コミット済み**だったが、Rebuild の Clean で消される罠があった。
- **MIDL（ミドル）**：Microsoft Interface Definition Language。COM インターフェースの定義から
  ヘッダを生成する仕組み。`d3d12shader.h` などがこれ由来で、`#undef small` で壊れた原因。
- **nlohmann/json（ヌロマン・ジェイソン）**：`json.hpp` 1ファイルで使える C++ の JSON ライブラリ。
  巨大で重いので PCH に入れる効果が大きい。
- **premake（プリメイク）**：`premake5.lua` から VS のソリューション（.sln/.vcxproj）を生成する
  ビルド構成ツール。
- **toolset v145（ツールセット ブイいちよんご）**：MSVC コンパイラのバージョン世代（VS18 / MSVC 14.5x）。
  ここでは FASTLINK 廃止の話に関係した。

### エラーコード（今回登場したもの）

- **9009（きゅうまるまるきゅう）**：コマンドプロンプトの「コマンドが見つからない」エラー。
  `fxc` が PATH に無くて出た。
- **C2513**：コンパイルエラー。「型名の後に識別子が無い」。`bool near` の `near` がマクロで
  消えて `bool ;` になったのが原因。
- **LNK1181**：リンクエラー。「入力ファイル（.lib）を開けない」。lib の検索パス不足で出る。
- **LNK4315**：リンカ警告。「`/DEBUG:FASTLINK` はもうサポートしない」。

### その他

- **CI（シーアイ / 継続的インテグレーション）**：push のたびに自動でビルド/テストする仕組み
  （GitHub Actions など）。本プロジェクトには現在無いため、.lib 差し替えの障害にならなかった。
- **Edit & Continue / E&C（エディットアンドコンティニュー）**：デバッグ実行を止めずに
  ソースを書き換えて反映する機能。`/ZI` が必要。今回は未使用だったので切った。

---

## 参考リンク（公式ドキュメント）

作業中に参照した一次情報。

### MSVC コンパイラ / リンカ オプション（Microsoft Learn）

- **`/Z7`, `/Zi`, `/ZI`（デバッグ情報フォーマット）** ★今回 `/ZI`→`/Zi` の根拠
  https://learn.microsoft.com/en-us/cpp/build/reference/z7-zi-zi-debug-information-format
- **`/DEBUG`（デバッグ情報の生成 / FULL・FASTLINK）**
  https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info
- **`/INCREMENTAL`（増分リンク）**
  https://learn.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally
- **`/MP`（複数プロセスでビルド）**
  https://learn.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes
- **`/MD`, `/MT`, `/LD`（ランタイムライブラリの選択）**
  https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library

### プリコンパイルヘッダ（PCH）

- **Precompiled Header Files（概説 / Microsoft Learn）**
  https://learn.microsoft.com/en-us/cpp/build/creating-precompiled-header-files
- **`/Yc`（PCH を生成）**
  https://learn.microsoft.com/en-us/cpp/build/reference/yc-create-precompiled-header-file
- **`/Yu`（PCH を使用）**
  https://learn.microsoft.com/en-us/cpp/build/reference/yu-use-precompiled-header-file
- **`/FI`（強制インクルード）** ★`forceincludes` の実体
  https://learn.microsoft.com/en-us/cpp/build/reference/fi-name-forced-include-file

### Premake（ビルド構成ツール）

- **公式サイト / ドキュメント**：https://premake.github.io/ ／ https://premake.github.io/docs/
- **Precompiled Headers（PCH の設定方法）**：https://premake.github.io/docs/Precompiled-Headers/
- **`pchheader`**：https://premake.github.io/docs/pchheader/
- **`pchsource`**：https://premake.github.io/docs/pchsource/
- **`forceincludes`**：https://premake.github.io/docs/forceincludes/
- **`editandcontinue`** ★`/ZI`↔`/Zi` の切替：https://premake.github.io/docs/editandcontinue/

### 外部ライブラリ / ツール

- **DirectXTex（Microsoft）**：https://github.com/microsoft/DirectXTex
- **Dear ImGui（ocornut）**：https://github.com/ocornut/imgui
- **nlohmann/json**：https://github.com/nlohmann/json
- **fxc（Effect-Compiler Tool / HLSL コンパイラ）**：https://learn.microsoft.com/en-us/windows/win32/direct3dtools/fxc
  - コマンド構文：https://learn.microsoft.com/en-us/windows/win32/direct3dtools/dx-graphics-tools-fxc-syntax
