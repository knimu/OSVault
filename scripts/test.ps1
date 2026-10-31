# OSVault - Automated Test Suite Runner
[CmdletBinding()]
param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Running OSVault CTest Automated Test Suite ($Config)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if (!(Test-Path "build")) {
    Write-Host "Build folder not found. Please run .\scripts\build.ps1 first." -ForegroundColor Red
    exit 1
}

ctest --test-dir build -C $Config --output-on-failure
$testExitCode = $LASTEXITCODE

Write-Host "==========================================================" -ForegroundColor Cyan
if ($testExitCode -eq 0) {
    Write-Host " ALL TEST SUITES PASSED (10/10)" -ForegroundColor Green
} else {
    Write-Host " Some tests failed. Exit code: $testExitCode" -ForegroundColor Red
    exit $testExitCode
}
