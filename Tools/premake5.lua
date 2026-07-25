-- =============================================================================
-- premake5.lua （Tools/ に配置）
--   このスクリプトは Tools/ にあるが、生成物(.sln)やパス解決は全て
--   リポジトリルート基準で行う。premake はパスをスクリプトのある場所基準で
--   解決するため、ルート基準にするには絶対パス化が必須。
-- =============================================================================

-- Tools/ の 1 つ上をリポジトリルートとして絶対パス化する
local root = path.getabsolute(_SCRIPT_DIR .. "/..")

-- ルート基準の絶対パスを作るヘルパー（/ 区切り）
local function r(p)  return root .. "/" .. p end
-- xcopy 引数用に \ 区切りへ変換するヘルパー
local function rw(p) return path.translate(root .. "/" .. p, "\\") end

-- 出力／中間ディレクトリ（$(SolutionDir) は .sln のある場所 = ルート）
local parentDir = path.getabsolute(root .. "/..")
local outputDir = parentDir .. "/generated/outputs/%{cfg.buildcfg}"
local intDir    = parentDir .. "/generated/intermediates/%{prj.name}/%{cfg.buildcfg}"

-- =============================================================================
-- ワークスペース定義
-- =============================================================================
workspace "YoRigine"
    architecture "x64"
    configurations { "Debug", "Develop", "Release" }
    platforms { "x64" }

    startproject "YMain" -- EXEプロジェクトを開始プロジェクトに設定
    location (root)      -- .sln をリポジトリルートに生成する

    language "C++"
    cppdialect "C++20"
    staticruntime "On"
    warnings "Extra"
    flags { "MultiProcessorCompile" }

    -- PlatformToolset (v145 = VS2026 のツールセット)
    toolset "v145"

    -- /FS: /MP(MultiProcessorCompile) で複数 cl.exe が同じ vc143.pdb へ書く際の
    --      書き込み競合(C1041)を防ぐ。並列ビルドや同時ビルドでも安全になる。
    buildoptions { "/utf-8", "/permissive-", "/FS" }
    defines { "NOMINMAX", "_WINDOWS" }

    targetdir (outputDir)
    objdir    (intDir)

    -- Debug と Develop は同じデバッグ設定 (symbols / _DEBUG)
    filter "configurations:Debug or Develop"
        defines { "_DEBUG" }
        symbols "On"

    -- Develop: Debug と同等のエディタ構成 + 起動シーンを DevelopScene にする。
    -- エンジン機能 (パーティクル/当たり判定/VFX) のテスト専用。
    -- Player を生成しないのでゲーム側のセーブは一切走らない。
    filter "configurations:Develop"
        defines { "DEVELOP_BUILD" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter {}

-- =============================================================================
-- インクルードパスのリスト定義（すべてルート基準の絶対パス）
-- =============================================================================
local engine_includes = {
    r"YEngine",
    r"YEngine/Core",
    r"YEngine/Core/DirectX",
    r"YEngine/Generators",
    r"YEngine/Graphics",
    r"YEngine/Systems",
    r"YEngine/Utilities",
    r"YEngine/Model",
    r"YMath",
    r"Externals/nlohmann",
    r"Externals/DirectXTex",
    r"Externals/imgui",
    r"Externals/assimp/include",
    r"Externals/icon",
    r"Externals/meshoptimizer/src",
    r"Externals/DirectXMesh/DirectXMesh"
}

local directx_libs = {
    "d3d12", "dxgi", "dxguid", "dxcompiler", "dinput8", "xinput"
}

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

--------------------------------------------------------------------------------
-- グループ: Externals (外部ライブラリ)
--------------------------------------------------------------------------------
group "Externals"

    --------------------- ImGui ---------------------
    project "ImGui"
        kind "StaticLib"
        language "C++"
        location (r"Externals/ImGui")
        warnings "Default"

        -- project location が Externals/ImGui でも、生成物は他プロジェクトと同じ
        -- D:/GameEngine/generated 以下へ集約する。ここで明示的に再指定して、
        -- 将来 workspace 設定を変更しても Externals/generated へ戻らないようにする。
        targetdir (outputDir)
        objdir    (intDir)

        files { r"Externals/ImGui/**.h", r"Externals/ImGui/**.cpp" }

        includedirs {
            r"Externals/ImGui",
            r"Externals/DirectXTex"
        }

    --------------------- DirectXTex (既存のvcxprojを参照) ---------------------
    externalproject "DirectXTex"
        location (r"Externals/DirectXTex")
        filename "DirectXTex_Desktop_2022_Win10"
        kind "StaticLib"
        language "C++"
        toolset "v145"
        -- 外部 vcxproj は Debug/Release しか持たないため Develop は Debug にマップ
        configmap { ["Develop"] = "Debug" }

    --------------------- DirectXMesh (既存のvcxprojを参照) ---------------------
    externalproject "DirectXMesh"
        location (r"Externals/DirectXMesh/DirectXMesh")
        filename "DirectXMesh_Desktop_2022_Win10"
        kind "StaticLib"
        language "C++"
        toolset "v145"
        configmap { ["Develop"] = "Debug" }

    --------------------- meshoptimizer ---------------------
    project "meshoptimizer"
        kind "StaticLib"
        language "C++"
        location (r"Externals/meshoptimizer")
        warnings "Default"

        files {
            r"Externals/meshoptimizer/src/meshoptimizer.h",
            r"Externals/meshoptimizer/src/**.cpp"
        }

        includedirs { r"Externals/meshoptimizer/src" }

--------------------------------------------------------------------------------
-- グループ: Engine (エンジン・コア)
--------------------------------------------------------------------------------
group "Engine"

    --------------------- YMath (Static Library) ---------------------
    project "YMath"
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"
        staticruntime "On"
        location (r"YMath")

        files {
            r"YMath/**.h",
            r"YMath/**.cpp"
        }

        includedirs {
            r"YMath"
        }

        vpaths {
            ["*"] = r"YMath/**"
        }

   --------------------- YEngine (Static Library) ---------------------
    project "YEngine"
        kind "StaticLib"
        location (r"YEngine")
        -- ※ GAME_BUILD_DLL は不要なら削除してください
        defines { "GAME_BUILD_DLL" }

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        files {
            r"YEngine/**.h",
            r"YEngine/**.cpp",
        }

        vpaths {
            ["YEngine/*"] = r"YEngine/**",
        }

        -- インクルードパス（ヘッダのみ。cURL を含む）
        includedirs {
            engine_includes,
            r"Externals/curl/include"
        }

        -- YEngine は静的ライブラリ。外部 lib を links するとその obj が
        -- YEngine.lib に丸ごとマージされ、最終リンクで LNK4006(重複)になる。
        -- よってここでは link せず、build 順序のための dependson のみ残す。
        -- 実際のリンクは最終バイナリ(Debug/Develop=YGame.dll / Release=YMain.exe)で行う。
        dependson { "YMath", "DirectXTex", "DirectXMesh", "meshoptimizer" }

        postbuildcommands {
            -- DXC/DXIL DLLのコピー
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}"',
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}"'
        }

        -- USE_IMGUI は YEngine のコンパイルに必要（#ifdef 分岐）。ImGui は
        -- ヘッダ参照のみ。lib リンクは最終バイナリ側。dependson は build 順序用。
        filter "configurations:Debug or Develop"
            defines { "USE_IMGUI" }
            dependson { "ImGui" }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }

        filter {}

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

        files {
            r"YGame/**.h",
            r"YGame/**.cpp"
        }
        removefiles { r"YGame/Main.cpp" }

        vpaths {
            ["YGame/*"] = r"YGame/**"
        }

        includedirs(game_includes)
        includedirs(engine_includes)

        dependson { "YEngine" }

        -- Debug / Develop: YGame は DLL = 最終バイナリ。
        -- YEngine が参照する全ライブラリをここでまとめてリンクする。
        filter "configurations:Debug or Develop"
            kind "SharedLib"
            defines { "GAME_BUILD_DLL" }  -- dllexport が有効
            defines { "USE_IMGUI" }
            dependson { "ImGui" }
            libdirs {
                outputDir,
                r"Externals/curl/lib",
                r"Externals/assimp/lib/Debug"
            }
            links {
                "YMath", "YEngine", "meshoptimizer", "ImGui",
                "DirectXTex.lib", "DirectXMesh.lib", "libcurl", "assimp-vc143-mtd"
            }
            links(directx_libs)

        -- Develop: DirectXTex/DirectXMesh は externalproject で Debug 構成に
        -- マップされ Debug フォルダへ出力されるため、そこも検索対象に追加。
        filter "configurations:Develop"
            libdirs { outputDir:gsub("%%{cfg.buildcfg}", "Debug") }

        -- Release: StaticLib として EXE に直接埋め込む。
        -- static なので外部 lib はここでリンクしない（マージ回避）。実リンクは YMain。
        -- GAME_BUILD_DLL 未定義 → GAME_API が空 → dllexport/import が消える
        filter "configurations:Release"
            kind "StaticLib"
            undefines { "USE_IMGUI" }
            removefiles { r"Externals/imgui/**.cpp" }

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
        includedirs(engine_includes)
        includedirs(game_includes)

        libdirs { outputDir }

        -- 共通のビルド後コマンドとしてここに記述
        postbuildcommands {
            'xcopy /Q /Y /I "' .. rw("Externals/curl/bin/libcurl.dll") .. '" "%{cfg.targetdir}"'
        }

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
            libdirs {
                outputDir,
                r"Externals/curl/lib",
                r"Externals/assimp/lib/Release"
            }
            links {
                "YGame", "YEngine", "YMath", "meshoptimizer",
                "DirectXTex.lib", "DirectXMesh.lib", "libcurl", "assimp-vc143-mt"
            }
            links(directx_libs)
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
