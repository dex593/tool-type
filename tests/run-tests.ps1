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
    if ($command) {
        return $command.Source
    }

    throw "Không tìm thấy $Name. Hãy cài MinGW-w64/MSYS2 và thêm thư mục bin vào PATH."
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$mingwBinFallbacks = @(
    "C:\Users\lsc\mingw32\bin",
    "C:\msys64\mingw64\bin",
    "C:\msys64\mingw32\bin"
)
$compiler = Resolve-TestTool "g++.exe" ($mingwBinFallbacks | ForEach-Object { Join-Path $_ "g++.exe" })
$testExe = Join-Path $PSScriptRoot "normalization_tests.exe"

& $compiler `
    -std=c++17 `
    -DUNICODE `
    -D_UNICODE `
    -Wall `
    -Wextra `
    -Wpedantic `
    (Join-Path $PSScriptRoot "normalization_tests.cpp") `
    -o $testExe `
    -static-libgcc `
    -static-libstdc++ `
    "-Wl,-Bstatic" `
    -lz `
    "-Wl,-Bdynamic" `
    -lcomdlg32 `
    -lgdi32 `
    -lshell32 `
    -luser32 `
    -lwininet

if ($LASTEXITCODE -ne 0) {
    throw "Biên dịch test thất bại với mã $LASTEXITCODE"
}

& $testExe
if ($LASTEXITCODE -ne 0) {
    throw "Normalization tests thất bại với mã $LASTEXITCODE"
}
