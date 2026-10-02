-- =============================================================================
-- premake5.lua （Tools/ に配置）
--   このスクリプトは Tools/ にあるが、生成物(.sln)やパス解決は全て
--   リポジトリルート基準で行う。premake はパスをスクリプトのある場所基準で
--   解決するため、ルート基準にするには絶対パス化が必須。
--
--   Engine（YEngine / YMath / Externals）の定義は Engine/Premake/engine.lua にある。
--   ここにはゲーム側（YGame / YMain / YResources）の定義だけを書く。
-- =============================================================================

-- Tools/ の 1 つ上をリポジトリルートとして絶対パス化する
local root = path.getabsolute(_SCRIPT_DIR .. "/..")

-- ルート基準の絶対パスを作るヘルパー（/ 区切り）
local function r(p)  return root .. "/" .. p end
-- xcopy 引数用に \ 区切りへ変換するヘルパー
local function rw(p) return path.translate(root .. "/" .. p, "\\") end

-- Engine サブモジュールの定義を読み込む（Engine 未取得なら分かりやすく止める）
local engineLua = r"Engine/Premake/engine.lua"
if not os.isfile(engineLua) then
    error("Engine/Premake/engine.lua が見つかりません。サブモジュール未取得の可能性があります:\n"
       .. "  git submodule update --init --recursive")
end
include(engineLua)
local Y = YoRigine
Y.init { root = root }

-- =============================================================================
-- ワークスペース定義
-- =============================================================================
workspace "YoRigine"
    startproject "YMain" -- EXEプロジェクトを開始プロジェクトに設定
    location (root)      -- .sln をリポジトリルートに生成する

    Y.workspaceDefaults()

-- =============================================================================
-- ゲーム側インクルードパスのリスト定義（すべてルート基準の絶対パス）
-- =============================================================================
local game_includes = {
    r"YGame",
    r"YGame/Core",
    r"YGame/Scenes",
    r"YGame/GameObjects",
    r"YGame/SystemsApp",
    r"YGame/UI"
}

-- =============================================================================
-- プロジェクト定義
-- =============================================================================

-- Engine 側（Externals: ImGui / DirectXMesh / meshoptimizer、Engine: YMath / YEngine）
Y.externals()
Y.engine()

--------------------------------------------------------------------------------
-- グループ: Game (ゲーム本体)
--------------------------------------------------------------------------------
group "Game"

    --------------------- YGame (Debug=DLL / Release=StaticLib) ---------------------
    project "YGame"
        -- kind は filter で設定
        location (r"YGame")

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        -- プリコンパイルヘッダ (コンパイル時間短縮)。YEngine と同方針。
        pchheader "pch.h"
        pchsource (r"YGame/pch.cpp")
        forceincludes { "pch.h" }

        files {
            r"YGame/**.h",
            r"YGame/**.cpp"
        }
        removefiles { r"YGame/Main.cpp" }

        vpaths {
            ["YGame/*"] = r"YGame/**"
        }

        includedirs(game_includes)
        includedirs(Y.engine_includes)

        dependson { "YEngine" }

        -- Debug / Develop: YGame は DLL = 最終バイナリ。
        -- YEngine が参照する全ライブラリをここでまとめてリンクする。
        filter "configurations:Debug or Develop"
            kind "SharedLib"
            defines { "GAME_BUILD_DLL" }  -- dllexport が有効
            defines { "USE_IMGUI" }
            dependson { "ImGui" }
            Y.linkDebug()

        -- Develop: DirectXTex/DirectXMesh は externalproject で Debug 構成に
        -- マップされ Debug フォルダへ出力されるため、そこも検索対象に追加。
        filter "configurations:Develop"
            libdirs { Y.outputDir:gsub("%%{cfg.buildcfg}", "Debug") }

        -- Release: StaticLib として EXE に直接埋め込む。
        -- static なので外部 lib はここでリンクしない（マージ回避）。実リンクは YMain。
        -- GAME_BUILD_DLL 未定義 → GAME_API が空 → dllexport/import が消える
        filter "configurations:Release"
            kind "StaticLib"
            undefines { "USE_IMGUI" }
            removefiles { Y.e"Externals/imgui/**.cpp" }

        filter {}

    --------------------- EXE (Windowed Application) ---------------------
    project "YMain"
        kind "WindowedApp"
        location (r"YMain")

        dependson { "YGame", "YResources" }

        debugdir (root)
        fatalwarnings { "All" }

        files { r"YMain/Main.cpp" }
        vpaths {
            ["YMain/*"] = r"YMain/**",
        }

        includedirs { root }
        includedirs(Y.engine_includes)
        includedirs(game_includes)

        libdirs { Y.outputDir }

        -- 共通のビルド後コマンド: Engine 同梱 DLL(libcurl)を出力先へコピー
        Y.copyRuntimeDlls()

        -- Debug/Develop: YGame は DLL。import lib(YGame.lib)だけリンクすれば、
        -- 実体(engine/外部lib)は DLL 側に含まれるので YMain は薄いまま。
        filter "configurations:Debug or Develop"
            defines { "_DEBUG" }
            defines { "GAME_IMPORT_DLL" }  -- YGame.dll のインポート宣言を有効化
            links { "YGame" }

        -- Release: 全プロジェクトが static。YMain(EXE)が最終リンクなので、
        -- engine・ゲーム・外部ライブラリを全てここでリンクする。
        filter "configurations:Release"
            defines { "NDEBUG" }
            Y.linkRelease { "YGame" }
            postbuildcommands {
                 'xcopy /Q /E /I /Y "' .. rw("Resources") .. '" "%{cfg.targetdir}/Resources"'
            }
            linkoptions { "/ignore:4006" }

        filter {}

--------------------------------------------------------------------------------
-- グループ終了
--------------------------------------------------------------------------------
group ""

--------------------------------------------------------------------------------
-- Resources (リソース管理)
--------------------------------------------------------------------------------
group "Resources"

    project "YResources"
        kind "None"
        location (r"Resources")

        files { r"Resources/**.*" }

        vpaths {
           ["Resources/*"] = r"Resources/**"
        }

        excludes { r"Resources/**.*" }

group ""
