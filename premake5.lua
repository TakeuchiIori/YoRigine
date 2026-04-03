-- 出力ディレクトリと中間ディレクトリの定義
local outputDir = "$(SolutionDir)../generated/outputs/%{cfg.buildcfg}"
local intDir    = "$(SolutionDir)../generated/intermediates/%{prj.name}/%{cfg.buildcfg}"

-- =============================================================================
-- Live++ のパス定義
-- =============================================================================
local livepp_dir = "%{wks.basedir}/Externals/LivePP"

-- =============================================================================
-- ワークスペース定義
-- =============================================================================
workspace "YoRigine"
    architecture "x64"
    configurations { "Debug", "Release" }
    platforms { "x64" }

    startproject "YMain"
    location "%{wks.basedir}" 
    
    language "C++"
    cppdialect "C++20"
    staticruntime "On"
    warnings "Extra"
    flags { "MultiProcessorCompile" }

    toolset "v143"
    
    buildoptions { "/utf-8", "/permissive-" }
    defines { "NOMINMAX", "_WINDOWS" }

    targetdir (outputDir)
    objdir    (intDir)

    -- ★★★ Live++ ドキュメント完全準拠のDebug設定 ★★★
    filter "configurations:Debug"
        defines { "_DEBUG" }
        symbols "On"          -- /Z7 または /Zi (デバッグ情報の生成)
        editandcontinue "Off" -- /ZI (エディットコンティニュ) は非対応のため無効化

        -- 【Live++: コンパイラ設定】
        buildoptions {
            "/Gm-", -- Enable Minimal Rebuild: No (最小リビルド無効化)
            "/Gy",  -- Enable Function-Level Linking: Yes (関数レベルリンク有効化 - 推奨)
            "/Gw"   -- Optimize Global Data: Yes (グローバルデータの最適化 - 推奨)
            -- ※ x64環境のため /hotpatch は不要です
        }

        -- 【Live++: リンカー設定】
        linkoptions {
            "/INCREMENTAL:NO", -- Enable Incremental Linking: No (インクリメンタルリンク無効化)
            "/OPT:NOREF",      -- References: Keep Unreferenced Data (未参照データを保持)
            "/OPT:NOICF",      -- Enable COMDAT Folding: No (COMDAT折りたたみ無効化)
            "/DEBUG:FULL" ,     -- FastLinkを無効化し、完全なPDBを生成する
            "/FUNCTIONPADMIN"
        }
    -- ★★★ ここまで ★★★

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
    "Externals/assimp/include",
    livepp_dir .. "/API"  -- 【追加】Live++のAPIヘッダー
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

group "Externals"

    externalproject "ImGui"
        location "Externals/ImGui"
        filename "ImGui"
        kind "StaticLib"
        language "C++"
        warnings "Default"

    externalproject "DirectXTex"
        location "Externals/DirectXTex"
        filename "DirectXTex_Desktop_2022_Win10"
        kind "StaticLib"
        language "C++"
        toolset "v143"

group "Engine"

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
        includedirs { "%{wks.basedir}/YMath" }
        vpaths { ["*"] = "YMath/**" }

    project "YEngine"
        kind "StaticLib"
        location "%{wks.basedir}/YEngine"
        defines { "GAME_BUILD_DLL" }

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        files { "YEngine/**.h", "YEngine/**.cpp" }
        vpaths { ["YEngine/*"] = "YEngine/**" }
        includedirs(engine_includes)
        dependson { "YMath","DirectXTex" }
        links { "YMath", "DirectXTex.lib" }

        postbuildcommands {
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}"',
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}"',
        }

        filter "configurations:Debug"
            defines { "USE_IMGUI" }
            dependson { "ImGui"}
            links { "ImGui" }
            libdirs { "Externals/assimp/lib/Debug", outputDir }
            links { "assimp-vc143-mtd" }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }
            libdirs { "Externals/assimp/lib/Release", outputDir }
            links { "assimp-vc143-mt" }
        filter {}

group "Game"

    project "YGame"
        kind "SharedLib"
        location "%{wks.basedir}/YGame"
        defines { "GAME_BUILD_DLL" }

        fatalwarnings { "All" }

        files { "YGame/**.h", "YGame/**.cpp" }
        removefiles { "YGame/Main.cpp" }
        vpaths { ["YGame/*"] = "YGame/**" }
        includedirs(game_includes)
        includedirs(engine_includes)

        dependson { "YEngine"}
        links { "YMath", "YEngine", "DirectXTex.lib" }
        links(directx_libs)

        filter "configurations:Debug"
            defines { "USE_IMGUI" }
            dependson { "ImGui"}
            links { "ImGui" }
            linkoptions { "/ignore:4099" }
            libdirs { outputDir }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }
            removefiles { "Externals/imgui/**.cpp" }
            libdirs { outputDir }
            linkoptions { "/ignore:4099" }
        filter {}

    project "YMain"
        kind "WindowedApp"
        location "%{wks.basedir}/YMain"

        dependson { "YGame" ,"YResources"}
        debugdir "%{wks.basedir}" 
        fatalwarnings { "All" }

        files { "YMain/Main.cpp" }
        vpaths { ["YMain/*"] = "YMain/**" }
        includedirs { "." }
        includedirs(engine_includes)
        includedirs(game_includes)
        
        postbuildcommands { } 

        filter "configurations:Debug"
            defines { "_DEBUG" }
            
        filter "configurations:Release"
            defines { "NDEBUG" }
            postbuildcommands {
                 'xcopy /Q /E /I /Y "%{wks.basedir}/Resources" "%{cfg.targetdir}/Resources"'
            }
            linkoptions { "/ignore:4006" }
        filter {}

group ""

group "Resources"
    project "YResources"
        kind "None" 
        location "Resources"
        files { "Resources/**.*" }
        vpaths { ["Resources/*"] = "Resources/**" }
        excludes { "Resources/**.*" }
group ""