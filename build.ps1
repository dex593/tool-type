$ErrorActionPreference = "Stop"

function Resolve-Tool {
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

$mingwBinFallbacks = @(
    "C:\Users\lsc\mingw32\bin",
    "C:\msys64\mingw64\bin",
    "C:\msys64\mingw32\bin"
)

foreach ($bin in $mingwBinFallbacks) {
    if (Test-Path $bin) {
        $env:PATH = "$bin;$env:PATH"
    }
}

$mingw = Resolve-Tool "g++.exe" ($mingwBinFallbacks | ForEach-Object { Join-Path $_ "g++.exe" })
$windres = Resolve-Tool "windres.exe" ($mingwBinFallbacks | ForEach-Object { Join-Path $_ "windres.exe" })

$resourceObj = "resource.res"
& $windres `
    .\resource.rc `
    -O coff `
    -o $resourceObj

if ($LASTEXITCODE -ne 0) {
    throw "Build resource thất bại với mã $LASTEXITCODE"
}

& $mingw `
    -std=c++17 `
    -O3 `
    -DNDEBUG `
    -DUNICODE `
    -D_UNICODE `
    -Wall `
    -Wextra `
    -Wpedantic `
    -municode `
    -mwindows `
    main.cpp `
    $resourceObj `
    -o ToolType.exe `
    "-Wl,--dynamicbase" `
    "-Wl,--nxcompat" `
    "-Wl,-Bstatic" `
    -lz `
    "-Wl,-Bdynamic" `
    -lcomdlg32 `
    -lgdi32 `
    -luser32 `
    -lwininet `
    -static-libgcc `
    -static-libstdc++

if ($LASTEXITCODE -ne 0) {
    throw "Build thất bại với mã $LASTEXITCODE"
}

Remove-Item -LiteralPath $resourceObj -ErrorAction SilentlyContinue

Write-Host "Build OK: $(Resolve-Path .\ToolType.exe)"
