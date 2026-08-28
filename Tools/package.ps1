#Requires -Version 5.1
<#
.SYNOPSIS
    YoRigine 配布用パッケージングスクリプト（build_release + extract_project を統合）。
    ※ビルドはしません。ビルド済みの成果物をコピー／整形するだけです。

.DESCRIPTION
    引数なしで実行するとメニューが出ます。
      [1] 実行ファイル : Release 出力をコピーし EXE を Goldin.exe にリネーム → ..\実行ファイル
      [2] プロジェクト : ソース／ソリューション一式を抽出           → ..\プロジェクト
      [3] 両方         : 上記を両方実行

.PARAMETER Mode
    Release / Project / Both  （省略でメニュー）

.PARAMETER Config
    実行ファイル生成に使うビルド構成（既定: Release）

.EXAMPLE
    .\package.ps1
    .\package.ps1 -Mode Release
    .\package.ps1 -Mode Both
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Project", "Both", "")]
    [string]$Mode = "",

    [ValidateSet("Debug", "Develop", "Release")]
    [string]$Config = "Release"
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# --- 名前の定義 ---------------------------------------------------------------
$ExeName      = 'Goldin.exe'    # 出力 EXE 名
$ReleaseDist  = '実行ファイル'  # バイナリ成果物フォルダ名
$ProjectDist  = 'プロジェクト'  # ソース抽出フォルダ名

# --- パス解決 -----------------------------------------------------------------
# このスクリプトは Tools\ にある。リポジトリルートはその 1 つ上、
# ビルド出力(generated) はさらに 1 つ上（premake5.lua の targetdir に一致）。
$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($ScriptDir)) { $ScriptDir = (Get-Location).Path }
$RepoRoot  = Split-Path $ScriptDir -Parent   # ...\YoRigine
$ParentDir = Split-Path $RepoRoot  -Parent   # ...\ (generated がある階層)

# --- 色ヘルパー ---------------------------------------------------------------
function Write-Header { param([string]$m)
    Write-Host ""; Write-Host ("=" * 58) -ForegroundColor Cyan
    Write-Host "  $m" -ForegroundColor Cyan
    Write-Host ("=" * 58) -ForegroundColor Cyan
}
function Write-Step { param([string]$m) Write-Host "[*] $m"  -ForegroundColor Yellow }
function Write-Ok   { param([string]$m) Write-Host "[OK] $m" -ForegroundColor Green  }
function Write-Fail { param([string]$m) Write-Host "[!!] $m" -ForegroundColor Red    }

function Select-FromMenu {
    param([string]$Title, [string[]]$Items)
    Write-Host ""; Write-Host $Title -ForegroundColor Cyan
    for ($i = 0; $i -lt $Items.Count; $i++) { Write-Host "  [$($i + 1)] $($Items[$i])" }
    Write-Host ""
    while ($true) {
        $raw = Read-Host "Enter number (1-$($Items.Count))"
        $num = 0
        if ([int]::TryParse($raw.Trim(), [ref]$num) -and $num -ge 1 -and $num -le $Items.Count) {
            return $Items[$num - 1]
        }
        Write-Host "  Invalid input. Please try again." -ForegroundColor Red
    }
}

# =============================================================================
# [1] 実行ファイル : Release 出力をコピー → EXE を Goldin.exe にリネーム
# =============================================================================
function Invoke-PackageRelease {
    $sourceDir = Join-Path $ParentDir ("generated\outputs\{0}" -f $Config)
    $distDir   = Join-Path $ParentDir $ReleaseDist

    Write-Header "実行ファイルを生成 ($Config)"
    Write-Host (" Source   : {0}" -f $sourceDir)
    Write-Host (" Dist     : {0}" -f $distDir)
    Write-Host (" Exe name : {0}" -f $ExeName)

    # 入力チェック
    if (-not (Test-Path $sourceDir)) {
        throw ("コピー元が見つかりません: {0}`n先に {1} ビルドを実行してください。" -f $sourceDir, $Config)
    }
    $builtExe = Join-Path $sourceDir 'YMain.exe'
    if (-not (Test-Path $builtExe)) {
        throw ("YMain.exe が見つかりません: {0}`n先に {1} ビルドを実行してください。" -f $builtExe, $Config)
    }

    # 配布フォルダをクリーンに作り直す
    Write-Step "成果物をコピー ..."
    if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
    New-Item -ItemType Directory -Path $distDir | Out-Null

    # EXE / DLL / Resources などをコピー。配布に不要な中間物は除外。
    $excludeExt = @('.lib', '.pdb', '.idb', '.exp', '.ilk')
    Get-ChildItem -Path $sourceDir -Force | ForEach-Object {
        if ($_.PSIsContainer) {
            Copy-Item $_.FullName -Destination $distDir -Recurse -Force
        } elseif ($excludeExt -notcontains $_.Extension.ToLower()) {
            Copy-Item $_.FullName -Destination $distDir -Force
        }
    }

    # EXE をリネーム
    Write-Step "EXE をリネーム ($ExeName) ..."
    $distExe  = Join-Path $distDir 'YMain.exe'
    $finalExe = Join-Path $distDir $ExeName
    if (Test-Path $finalExe) { Remove-Item $finalExe -Force }
    Rename-Item -Path $distExe -NewName $ExeName

    Write-Ok ("完了 : {0}" -f $finalExe)
}

# =============================================================================
# [2] プロジェクト : ソース／ソリューション一式を robocopy で抽出
# =============================================================================
function Invoke-ExtractProject {
    $sourceDir = $RepoRoot
    $distDir   = Join-Path $ParentDir $ProjectDist

    Write-Header "プロジェクト（ソース）を抽出"
    Write-Host (" Source : {0}" -f $sourceDir)
    Write-Host (" Dist   : {0}" -f $distDir)

    if (-not (Test-Path $sourceDir)) {
        throw ("コピー元が見つかりません: {0}" -f $sourceDir)
    }

    # 除外フォルダ（トップ階層の不要物。フルパス指定でその場所だけを除外）
    $excludeDirs = @(
        (Join-Path $sourceDir '.git'),
        (Join-Path $sourceDir '.vs'),
        (Join-Path $sourceDir '.github'),
        (Join-Path $sourceDir '.claude'),
        (Join-Path $sourceDir '.agents'),
        (Join-Path $sourceDir 'generated')
    )
    # 除外ファイル（どの階層でも除外）
    $excludeFiles = @(
        '.gitignore', '.gitattributes', '.gitmodules',
        'CLAUDE.md', 'exclusion.dic',
        'imgui.ini', 'editor_settings.ini',
        '*.user', '*.suo', '*.VC.db', '*.VC.opendb'
    )

    # 抽出先をクリーンに作り直す
    if (Test-Path $distDir) {
        Write-Host "既存の「$ProjectDist」を削除して作り直します ..." -ForegroundColor DarkGray
        Remove-Item $distDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $distDir | Out-Null

    Write-Step "robocopy でコピー中 ..."
    $rcArgs = @(
        $sourceDir, $distDir,
        '/E', '/XD'
    ) + $excludeDirs + @('/XF') + $excludeFiles + @(
        '/R:1', '/W:1', '/NFL', '/NDL', '/NP', '/NJH'
    )
    & robocopy @rcArgs
    $rc = $LASTEXITCODE
    # robocopy: 0-7 は成功、8 以上が失敗
    if ($rc -ge 8) { throw "robocopy が失敗しました (exit $rc)" }

    $fileCount = (Get-ChildItem -Path $distDir -Recurse -File -Force | Measure-Object).Count
    $sizeMB    = [math]::Round((Get-ChildItem -Path $distDir -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
    Write-Ok ("完了 (robocopy exit $rc) : {0} 個 / {1} MB" -f $fileCount, $sizeMB)
}

# =============================================================================
# Main
# =============================================================================
Write-Header "YoRigine Packaging"

if ($Mode -eq "") {
    $choice = Select-FromMenu -Title "生成するもの:" -Items @("実行ファイル", "プロジェクト", "両方")
    switch ($choice) {
        "実行ファイル" { $Mode = "Release" }
        "プロジェクト" { $Mode = "Project" }
        "両方"         { $Mode = "Both" }
    }
}

$startTime = Get-Date
switch ($Mode) {
    "Release" { Invoke-PackageRelease }
    "Project" { Invoke-ExtractProject }
    "Both"    { Invoke-PackageRelease; Invoke-ExtractProject }
}
$elapsed = (Get-Date) - $startTime

Write-Host ""
Write-Host ("=" * 58) -ForegroundColor Green
Write-Ok ("すべて完了  (elapsed: {0})" -f $elapsed.ToString('mm\:ss'))
Write-Host ("=" * 58) -ForegroundColor Green
Write-Host ""
