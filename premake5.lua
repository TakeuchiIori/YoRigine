-- 出力ディレクトリと中間ディレクトリの定義
local outputDir = "$(SolutionDir)../generated/outputs/%{cfg.buildcfg}"
local intDir    = "$(SolutionDir)../generated/intermediates/%{prj.name}/%{cfg.buildcfg}"

-- =============================================================================
-- ワークスペース定義
-- =============================================================================
workspace "YoRigine"
    architecture "x64"
    configurations { "Debug", "Release" }
    platforms { "x64" }

    startproject "YMain" -- EXEプロジェクトを開始プロジェクトに設定
    location "%{wks.basedir}" 
    
    language "C++"
    cppdialect "C++20"
    staticruntime "On"
    warnings "Extra"
    flags { "MultiProcessorCompile" }

    -- PlatformToolset
    toolset "v145"
    
    buildoptions { "/utf-8", "/permissive-" }
    defines { "NOMINMAX", "_WINDOWS" }

    targetdir (outputDir)
    objdir    (intDir)

    filter "configurations:Debug"
        defines { "_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter {}

-- =============================================================================
-- インクルードパスのリスト定義
-- =============================================================================
local engine_includes = {
    "YEngine",
    "YEngine/Core",
    "YEngine/Core/DirectX",
    "YEngine/Generators",
    "YEngine/Graphics",
    "YEngine/Systems",
    "YEngine/Utilities",
    "YEngine/Model",
    "YMath",
    "Externals/nlohmann",
    "Externals/DirectXTex",
    "Externals/imgui",
    "Externals/assimp/include"
}

local directx_libs = {
    "d3d12", "dxgi", "dxguid", "dxcompiler", "dinput8", "xinput"
}

local game_includes = {
    "YGame",
    "YGame/Core",
    "YGame/Scenes",
    "YGame/GameObjects",
    "YGame/SystemsApp",
    "YGame/UI"
}

-- =============================================================================
-- プロジェクト定義
-- =============================================================================

--------------------------------------------------------------------------------
-- グループ: Externals (外部ライブラリ)
--------------------------------------------------------------------------------
group "Externals"

    --------------------- ImGui (既存のvcxprojを参照) ---------------------
    externalproject "ImGui"
        location "Externals/ImGui"
        filename "ImGui"
        kind "StaticLib"
        language "C++"
        warnings "Default"

    --------------------- DirectXTex (既存のvcxprojを参照) ---------------------
    externalproject "DirectXTex"
        location "Externals/DirectXTex"
        filename "DirectXTex_Desktop_2022_Win10"
        kind "StaticLib"
        language "C++"

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
        location "%{wks.basedir}/YMath"

        files {
            "%{wks.basedir}/YMath/**.h",
            "%{wks.basedir}/YMath/**.cpp"
        }

        includedirs {
            "%{wks.basedir}/YMath"
        }

        vpaths {
            ["*"] = "YMath/**"
        }

   --------------------- YEngine (Static Library) ---------------------
    project "YEngine"
        kind "StaticLib"
        location "%{wks.basedir}/YEngine"
        -- ※ GAME_BUILD_DLL は不要なら削除してください
	defines { "GAME_BUILD_DLL" }

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        files {
            "YEngine/**.h",
            "YEngine/**.cpp",
        }
        
        vpaths {
            ["YEngine/*"] = "YEngine/**",
        }

        -- インクルードパス（cURLを追加）
        includedirs {
            engine_includes,
            "Externals/curl/include"
        }
        
        -- ライブラリとリンク設定（cURLを追加）
        libdirs { "Externals/curl/lib" }
        links { "YMath", "DirectXTex.lib", "libcurl" }

        dependson { "YMath","DirectXTex" }

        postbuildcommands {
            -- DXC/DXIL DLLのコピー（タイポ修正済み）
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}"',
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}"'
        }

        filter "configurations:Debug"
            defines { "USE_IMGUI" }
            dependson { "ImGui"}
            links { "ImGui" }
            libdirs { 
                "Externals/assimp/lib/Debug",
                outputDir
            }
            links { "assimp-vc143-mtd" }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }
            libdirs { 
                "Externals/assimp/lib/Release",
                outputDir
            }
            links { "assimp-vc143-mt" }

        filter {}

--------------------------------------------------------------------------------
-- グループ: Game (ゲーム本体)
--------------------------------------------------------------------------------
group "Game"

    --------------------- YGame (Debug=DLL / Release=StaticLib) ---------------------
    project "YGame"
        -- kind は filter で設定
        location "%{wks.basedir}/YGame"

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        files {
            "YGame/**.h",
            "YGame/**.cpp"
        }
        removefiles { "YGame/Main.cpp" }

        vpaths {
            ["YGame/*"] = "YGame/**"
        }

        includedirs(game_includes)
        includedirs(engine_includes)

        dependson { "YEngine"}
        links { "YMath", "YEngine", "DirectXTex.lib" }
        links(directx_libs)

        -- Debug: DLL として配布・暗黙的リンク
        filter "configurations:Debug"
            kind "SharedLib"
            defines { "GAME_BUILD_DLL" }  -- dllexport が有効
            defines { "USE_IMGUI" }
            dependson { "ImGui" }
            links { "ImGui" }
            libdirs { outputDir }

        -- Release: StaticLib として EXE に直接埋め込む
        -- GAME_BUILD_DLL 未定義 → GAME_API が空 → dllexport/import が消える
        filter "configurations:Release"
            kind "StaticLib"
            undefines { "USE_IMGUI" }
            removefiles { "Externals/imgui/**.cpp" }
            libdirs { outputDir }

        filter {}

    --------------------- EXE (Windowed Application) ---------------------
    project "YMain"
        kind "WindowedApp"
        location "%{wks.basedir}/YMain"

        dependson { "YGame", "YResources" }
        
        debugdir "%{wks.basedir}" 
        fatalwarnings { "All" }

        files { "YMain/Main.cpp" }
        vpaths {
            ["YMain/*"] = "YMain/**",
        }

        includedirs { "." }
        includedirs(engine_includes)
        includedirs(game_includes)

        -- YGame.lib（インポートライブラリ）を暗黙的リンク
        libdirs { outputDir }
        links { "YGame" }

        -- 共通のビルド後コマンドとしてここに記述
        postbuildcommands {
            'xcopy /Q /Y /I "%{wks.basedir}\\Externals\\curl\\bin\\libcurl.dll" "%{cfg.targetdir}"'
        }

        filter "configurations:Debug"
            defines { "_DEBUG" }
            defines { "GAME_IMPORT_DLL" }  -- YGame.dll のインポート宣言を有効化
        
        filter "configurations:Release"
            defines { "NDEBUG" }
            postbuildcommands {
                 'xcopy /Q /E /I /Y "%{wks.basedir}/Resources" "%{cfg.targetdir}/Resources"'
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
        location "Resources"
        
        files { "Resources/**.*" }
        
        vpaths {
           ["Resources/*"] = "Resources/**"
        }

        excludes { "Resources/**.*" }

group ""