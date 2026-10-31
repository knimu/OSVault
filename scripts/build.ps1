# OSVault - Build Automation Script
[CmdletBinding()]
param(
    [string]$Config = "Release",
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " OSVault Build Automation ($Config)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

if (!(Test-Path "build/CMakeCache.txt")) {
    Write-Host "Configuring CMake project..." -ForegroundColor Cyan
    cmake -B build -S .
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

Write-Host "Compiling OSVault targets..." -ForegroundColor Cyan
cmake --build build --config $Config
if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "==========================================================" -ForegroundColor Green
Write-Host " Build succeeded! Executable located at: build/$Config/osvault.exe" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
