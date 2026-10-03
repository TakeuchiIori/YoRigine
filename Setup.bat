@echo off
chcp 65001 > nul
cd /d "%~dp0"

echo [1/3] Engine サブモジュールを確認...
if exist "Engine\Premake\engine.lua" (
    echo       取得済みのためスキップします。
) else (
    echo       未取得のため取得します...
    git submodule update --init --recursive || goto :error
)

echo [2/3] git フックを確認...
set "CUR_HOOKS="
for /f "delims=" %%H in ('git config --get core.hooksPath') do set "CUR_HOOKS=%%H"
if "%CUR_HOOKS%"=="Tools/githooks" (
    echo       有効化済みのためスキップします。
) else (
    echo       有効化します...
    git config core.hooksPath Tools/githooks || goto :error
)

echo [3/3] Premake でソリューションを生成...
call Tools\premake.bat || goto :error

echo.
echo 完了: YoRigine.sln を開いてください。
pause
exit /b 0

:error
echo.
echo 失敗しました。上のメッセージを確認してください。
pause
exit /b 1