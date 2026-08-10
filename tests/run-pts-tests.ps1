$ErrorActionPreference = "Stop"

function Resolve-TestTool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string[]]$FallbackPaths = @()
    )

    foreach ($path in $FallbackPaths) {
        if ($path -and (Test-Path $path)) {
            return (Resolve-Path $path).Path
        }
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw "Không tìm thấy $Name."
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$mingwBinFallbacks = @(
    "C:\Users\lsc\mingw32\bin",
    "C:\msys64\mingw64\bin",
    "C:\msys64\mingw32\bin"
)
$compiler = Resolve-TestTool "g++.exe" ($mingwBinFallbacks | ForEach-Object {
    Join-Path $_ "g++.exe"
})
$testExe = Join-Path $PSScriptRoot "pts_regression_tests.exe"

& $compiler `
    -std=c++17 `
    -O0 `
    -DUNICODE `
    -D_UNICODE `
    -Wall `
    -Wextra `
    -Wpedantic `
    (Join-Path $PSScriptRoot "pts_regression_tests.cpp") `
    (Join-Path $repoRoot "text_normalization.cpp") `
    -o $testExe `
    -lz `
    -lcomdlg32 `
    -ladvapi32 `
    -lgdi32 `
    -lshell32 `
    -luser32 `
    -lwininet `
    -static-libgcc `
    -static-libstdc++

if ($LASTEXITCODE -ne 0) {
    throw "Biên dịch Pts regression tests thất bại với mã $LASTEXITCODE"
}

& $testExe
if ($LASTEXITCODE -ne 0) {
    throw "Pts regression tests thất bại với mã $LASTEXITCODE"
}
