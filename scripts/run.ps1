# OSVault - Server Execution Script
[CmdletBinding()]
param(
    [int]$Port = 8080,
    [string]$Config = "Release",
    [string]$DbPath = "data/osvault.db",
    [string]$WebDir = "web"
)

$ErrorActionPreference = "Stop"

$exePath = "build/$Config/osvault.exe"
if (!(Test-Path $exePath)) {
    # Check alternate location
    $exePath = "build/osvault.exe"
    if (!(Test-Path $exePath)) {
        Write-Host "Binary not found. Please run .\scripts\build.ps1 first." -ForegroundColor Red
        exit 1
    }
}

if (!(Test-Path "data")) {
    New-Item -ItemType Directory -Path "data" -Force | Out-Null
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Starting OSVault Server" -ForegroundColor Cyan
Write-Host " URL: http://localhost:$Port" -ForegroundColor Green
Write-Host " Database: $DbPath" -ForegroundColor DarkGray
Write-Host " Web Dir:  $WebDir" -ForegroundColor DarkGray
Write-Host " Press Ctrl+C to stop the server" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

& $exePath --port $Port --db $DbPath --web $WebDir
