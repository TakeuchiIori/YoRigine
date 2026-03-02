
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
    toolset "v143"
    
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
        toolset "v143"

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

        includedirs(engine_includes)
        
        dependson { "YMath","DirectXTex" }
        links { "YMath", "DirectXTex.lib" }

        postbuildcommands {
            -- DXC/DXIL DLLのコピーは引き続き行う
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}"',
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}"',
        }

        filter "configurations:Debug"
            defines { "USE_IMGUI" }
            dependson { "ImGui"}
            links { "ImGui" } -- 外部プロジェクト名と合わせる
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

    --------------------- GameDll (Shared Library) ---------------------
    project "YGame"
        kind "SharedLib"
        location "%{wks.basedir}/YGame"
        defines { "GAME_BUILD_DLL" }

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

        filter "configurations:Debug"
            defines { "USE_IMGUI" }
            dependson { "ImGui"}
            links { "ImGui" }
            libdirs { outputDir }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }
            removefiles { "Externals/imgui/**.cpp" }
            libdirs { outputDir }

        filter {}

    --------------------- EXE (Windowed Application) ---------------------
    project "YMain"
        kind "WindowedApp"
        location "%{wks.basedir}/YMain"

        -- 先にGame側をビルド
        dependson { "YGame" ,"YResources"}
        
        -- 【変更点 1】デバッグ時の作業ディレクトリをワークスペースルートに設定
        -- これにより、EXEはここで実行され、"Resources"はソースフォルダを指す
        debugdir "%{wks.basedir}" 
        fatalwarnings { "All" }

        files { "YMain/Main.cpp" }
        vpaths {
            ["YMain/*"] = "YMain/**",
        }

        includedirs { "." }
        includedirs(engine_includes)
        includedirs(game_includes)
        
        -- 【変更点 2】共通のpostbuildcommandsからリソースコピーを削除
        -- DXC/DXIL DLLのコピーはYEngineに移したので、ここは空でOK
        postbuildcommands { } 

        filter "configurations:Debug"
            defines { "_DEBUG" }
            -- Debug時はリソースコピー処理なし (元のフォルダを直接読み書き)
        
        filter "configurations:Release"
            defines { "NDEBUG" }
            -- 【変更点 3】Release時のみリソースをEXEの隣にコピー
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