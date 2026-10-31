# OSVault - Environment Setup & Verification Script
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " OSVault Environment Setup & Dependency Verification" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$allPass = $true

# 1. Check CMake
Write-Host "[1/5] Checking CMake..." -NoNewline
try {
    $cmakeVer = cmake --version | Select-Object -First 1
    Write-Host " [OK] $cmakeVer" -ForegroundColor Green
} catch {
    Write-Host " [FAIL] CMake not found in PATH." -ForegroundColor Red
    $allPass = $false
}

# 2. Check Git
Write-Host "[2/5] Checking Git..." -NoNewline
try {
    $gitVer = git --version
    Write-Host " [OK] $gitVer" -ForegroundColor Green
} catch {
    Write-Host " [FAIL] Git not found in PATH." -ForegroundColor Red
    $allPass = $false
}

# 3. Check C++ Compiler / Build Tools
Write-Host "[3/5] Checking C++ Compiler..." -NoNewline
$clPath = Get-Command cl -ErrorAction SilentlyContinue
if ($clPath) {
    Write-Host " [OK] MSVC cl.exe detected at $($clPath.Source)" -ForegroundColor Green
} else {
    $msbuildPath = Get-Command MSBuild -ErrorAction SilentlyContinue
    if ($msbuildPath) {
        Write-Host " [OK] MSBuild detected at $($msbuildPath.Source)" -ForegroundColor Green
    } else {
        Write-Host " [WARN] Neither cl.exe nor MSBuild found in current PATH. Ensure Developer Command Prompt or Visual Studio Build Tools are installed." -ForegroundColor Yellow
    }
}

# 4. Check Third-Party Dependencies in third_party/
Write-Host "[4/5] Checking pinned third-party dependencies..."
$deps = @(
    @{ Name = "Crow C++ Header"; Path = "third_party/crow/crow_all.h" },
    @{ Name = "Standalone Asio"; Path = "third_party/asio/include/asio.hpp" },
    @{ Name = "SQLite Amalgamation C"; Path = "third_party/sqlite/sqlite3.c" },
    @{ Name = "SQLite Amalgamation Header"; Path = "third_party/sqlite/sqlite3.h" },
    @{ Name = "doctest Test Framework"; Path = "third_party/doctest/doctest.h" }
)

foreach ($dep in $deps) {
    Write-Host "  - $($dep.Name)... " -NoNewline
    if (Test-Path $dep.Path) {
        Write-Host "[OK]" -ForegroundColor Green
    } else {
        Write-Host "[MISSING] at $($dep.Path)" -ForegroundColor Red
        $allPass = $false
    }
}

# 5. Check Directory Structure
Write-Host "[5/5] Checking workspace directory structure..."
$dirs = @("include/osvault", "src/routes", "tests", "web/assets/css", "web/assets/js", "docs", "scripts")
foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "  Created directory: $dir" -ForegroundColor DarkGray
    }
}
Write-Host "  [OK] Directory hierarchy intact." -ForegroundColor Green

Write-Host "==========================================================" -ForegroundColor Cyan
if ($allPass) {
    Write-Host " Environment ready. Run .\scripts\build.ps1 to build OSVault." -ForegroundColor Green
} else {
    Write-Host " Missing dependencies or tools detected. Please address errors before building." -ForegroundColor Red
    exit 1
}
