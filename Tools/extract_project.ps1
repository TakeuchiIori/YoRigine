<#
.SYNOPSIS
    ソースコード・ソリューション一式を「プロジェクト」フォルダに抽出（コピー）します。
    ビルドはしません。ビルド可能な一式をコピーするだけです。

.DESCRIPTION
    リポジトリの中身を「プロジェクト」フォルダへ robocopy でコピーします。
    含めるもの : 自作コード(YEngine/YGame/YMain/YMath)、ソリューション(.sln/.vcxproj)、
                 premake、Externals、Resources など。
    除外するもの: .git / .vs / .github / .claude / generated（ビルド出力）、
                 .gitignore / .gitattributes / CLAUDE.md、
                 各種 VS ユーザー設定・ランタイム生成ファイルなどの不要物。

.PARAMETER SourceDir
    コピー元（既定: このスクリプトのある場所＝リポジトリ直下）。

.PARAMETER DistDir
    コピー先フォルダ（既定: リポジトリの一つ上の階層の「プロジェクト」）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\extract_project.ps1
#>

[CmdletBinding()]
param(
    [string]$SourceDir,
    [string]$DistDir
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# --- 名前の定義 ---------------------------------------------------------------
$DistName = 'プロジェクト'   # 抽出先フォルダ名

# --- パス解決 -----------------------------------------------------------------
# このスクリプトはリポジトリ直下 (YoRigine.sln と同じ場所) に置く想定
$RepoRoot = $PSScriptRoot
if ([string]::IsNullOrEmpty($RepoRoot)) { $RepoRoot = (Get-Location).Path }
$ParentDir = Split-Path $RepoRoot -Parent

if (-not $SourceDir) { $SourceDir = $RepoRoot }
if (-not $DistDir)   { $DistDir   = Join-Path $ParentDir $DistName }  # 一つ上の階層に作成

# --- 除外リスト ---------------------------------------------------------------
# 除外フォルダ（トップ階層の不要物。フルパス指定でその場所だけを除外）
$excludeDirs = @(
    (Join-Path $SourceDir '.git'),
    (Join-Path $SourceDir '.vs'),
    (Join-Path $SourceDir '.github'),    # CI 設定
    (Join-Path $SourceDir '.claude'),    # Claude 関連
    (Join-Path $SourceDir 'generated')   # ビルド出力・中間ファイル
)

# 除外ファイル（名前 / ワイルドカードで指定。どの階層でも除外）
$excludeFiles = @(
    '.gitignore', '.gitattributes', '.gitmodules',
    'CLAUDE.md',                          # Claude 関連
    'exclusion.dic',
    'imgui.ini', 'editor_settings.ini',   # ランタイム生成
    '*.user', '*.suo', '*.VC.db', '*.VC.opendb'
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " ソース／ソリューションを「$DistName」へ抽出" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host (" Source : {0}" -f $SourceDir)
Write-Host (" Dist   : {0}" -f $DistDir)
Write-Host ""

# --- 入力チェック -------------------------------------------------------------
if (-not (Test-Path $SourceDir)) {
    throw ("コピー元が見つかりません: {0}" -f $SourceDir)
}

# 抽出先をクリーンに作り直す
if (Test-Path $DistDir) {
    Write-Host "既存の「$DistName」を削除して作り直します ..." -ForegroundColor DarkGray
    Remove-Item $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path $DistDir | Out-Null

# --- robocopy でコピー --------------------------------------------------------
Write-Host "robocopy でコピー中 ..." -ForegroundColor Yellow

$rcArgs = @(
    $SourceDir,
    $DistDir,
    '/E',            # サブフォルダ（空も含む）をコピー
    '/XD'
) + $excludeDirs + @('/XF') + $excludeFiles + @(
    '/R:1', '/W:1',  # 失敗時リトライ1回・待機1秒
    '/NFL', '/NDL',  # ファイル名・フォルダ名のログ抑制
    '/NP',           # 進捗%抑制
    '/NJH'           # ジョブヘッダ抑制
)

& robocopy @rcArgs
$rc = $LASTEXITCODE

# robocopy の終了コードは 0-7 が成功、8 以上が失敗
if ($rc -ge 8) {
    throw "robocopy が失敗しました (exit $rc)"
}

# --- 結果サマリ ---------------------------------------------------------------
$fileCount = (Get-ChildItem -Path $DistDir -Recurse -File -Force | Measure-Object).Count
$sizeMB    = [math]::Round((Get-ChildItem -Path $DistDir -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum / 1MB, 1)

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Green
Write-Host " 完了 (robocopy exit $rc)" -ForegroundColor Green
Write-Host ("   フォルダ : {0}" -f $DistDir) -ForegroundColor Green
Write-Host ("   ファイル : {0} 個 / {1} MB" -f $fileCount, $sizeMB) -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
