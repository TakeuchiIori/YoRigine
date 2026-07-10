#Requires -Version 5.1
<#
.SYNOPSIS
    YoRigine Game Engine - MSBuild Script

.DESCRIPTION
    Builds YoRigine.sln via MSBuild.
    Run without arguments to get an interactive menu.

.PARAMETER Config
    Build configuration: Debug / Develop / Release  (omit for menu)

.PARAMETER Action
    Build action: Build / Rebuild / Clean  (omit for Build)

.PARAMETER MaxCpu
    Number of CPU cores for parallel build  (omit for auto)

.EXAMPLE
    .\Build.ps1
    .\Build.ps1 -Config Debug
    .\Build.ps1 -Config Release -Action Rebuild
    .\Build.ps1 -Config Develop -Action Clean
#>

param(
    [ValidateSet("Debug", "Develop", "Release", "")]
    [string]$Config = "",

    [ValidateSet("Build", "Rebuild", "Clean", "")]
    [string]$Action = "",

    [int]$MaxCpu = 0
)

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------
# Constants
# ------------------------------------------------------------
$SLN_PATH = Join-Path $PSScriptRoot "YoRigine.sln"
$PLATFORM  = "x64"
$VSWHERE   = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

# ------------------------------------------------------------
# Color helpers
# ------------------------------------------------------------
function Write-Header {
    param([string]$msg)
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host "  $msg"   -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor Cyan
}

function Write-Step {
    param([string]$msg)
    Write-Host "[*] $msg" -ForegroundColor Yellow
}

function Write-Ok {
    param([string]$msg)
    Write-Host "[OK] $msg" -ForegroundColor Green
}

function Write-Fail {
    param([string]$msg)
    Write-Host "[!!] $msg" -ForegroundColor Red
}

# ------------------------------------------------------------
# Find MSBuild
# ------------------------------------------------------------
function Find-MSBuild {
    # 1. Use vswhere to locate the latest VS installation
    if (Test-Path $VSWHERE) {
        $vsPath = & $VSWHERE -latest -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    # 2. Fixed-path fallbacks (VS2022 / VS2019)
    $fallbacks = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($fb in $fallbacks) {
        if (Test-Path $fb) {
            return $fb
        }
    }

    return $null
}

# ------------------------------------------------------------
# Simple menu selector
# ------------------------------------------------------------
function Select-FromMenu {
    param(
        [string]   $Title,
        [string[]] $Items
    )

    Write-Host ""
    Write-Host $Title -ForegroundColor Cyan
    for ($i = 0; $i -lt $Items.Count; $i++) {
        Write-Host "  [$($i + 1)] $($Items[$i])"
    }
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

# ============================================================
# Main
# ============================================================
Write-Header "YoRigine MSBuild Script"

# --- Verify solution exists ---
if (-not (Test-Path $SLN_PATH)) {
    Write-Fail "Solution not found: $SLN_PATH"
    exit 1
}

# --- Locate MSBuild ---
Write-Step "Searching for MSBuild..."
$msbuildExe = Find-MSBuild
if (-not $msbuildExe) {
    Write-Fail "MSBuild not found. Please install Visual Studio 2019 or 2022."
    exit 1
}
Write-Ok "MSBuild : $msbuildExe"

# --- Select configuration ---
if ($Config -eq "") {
    $Config = Select-FromMenu -Title "Select build configuration:" -Items @("Debug", "Develop", "Release")
}
Write-Ok "Config  : $Config|$PLATFORM"

# --- Select action ---
if ($Action -eq "") {
    $Action = Select-FromMenu -Title "Select action:" -Items @("Build", "Rebuild", "Clean")
}
Write-Ok "Action  : $Action"

# --- CPU cores ---
if ($MaxCpu -le 0) {
    $MaxCpu = [Environment]::ProcessorCount
}
Write-Ok "Cores   : $MaxCpu"

# --- Run MSBuild ---
Write-Header "Starting $Action  ($Config|$PLATFORM)"

$startTime = Get-Date

$msbuildArgs = @(
    $SLN_PATH,
    "/t:$Action",
    "/p:Configuration=$Config",
    "/p:Platform=$PLATFORM",
    "/m:$MaxCpu",
    "/nologo",
    "/consoleloggerparameters:Summary;ForceNoAlign",
    "/verbosity:minimal"
)

Write-Step "Command: MSBuild $($msbuildArgs -join ' ')"
Write-Host ""

& $msbuildExe @msbuildArgs

$exitCode = $LASTEXITCODE
$elapsed  = (Get-Date) - $startTime

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor Cyan
if ($exitCode -eq 0) {
    Write-Ok "$Action succeeded  (elapsed: $($elapsed.ToString('mm\:ss')))"
} else {
    Write-Fail "$Action FAILED  (exit code: $exitCode  elapsed: $($elapsed.ToString('mm\:ss')))"
}
Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host ""

exit $exitCode
