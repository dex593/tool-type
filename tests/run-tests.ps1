param(
    [switch]$Coverage
)

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
$outputDirectory = $PSScriptRoot
$testName = "normalization_tests"
if ($Coverage) {
    $outputDirectory = Join-Path $PSScriptRoot "coverage"
    $testName = "normalization_coverage"
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$testExe = Join-Path $outputDirectory "$testName.exe"
$normalizationSource = Join-Path $repoRoot "text_normalization.cpp"
$testSource = Join-Path $PSScriptRoot "normalization_tests.cpp"
if ($Coverage) {
    $normalizationSource = "..\..\text_normalization.cpp"
    $testSource = "..\normalization_tests.cpp"
}
$compilerArgs = @(
    "-std=c++17",
    "-DUNICODE",
    "-D_UNICODE",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    $normalizationSource,
    $testSource,
    "-o",
    $(if ($Coverage) { ".\$testName.exe" } else { $testExe }),
    "-static-libgcc",
    "-static-libstdc++"
)
if ($Coverage) {
    $compilerArgs += @("-O0", "--coverage")
} else {
    $compilerArgs += "-Wl,--gc-sections"
}

if ($Coverage) {
    Push-Location $outputDirectory
    try {
        foreach ($artifact in @(
            "normalization_coverage-normalization_tests.gcda",
            "normalization_coverage-normalization_tests.gcno",
            "normalization_coverage-text_normalization.gcda",
            "normalization_coverage-text_normalization.gcno",
            "text_normalization.cpp.gcov"
        )) {
            Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
        }
        & $compiler @compilerArgs
    } finally {
        Pop-Location
    }
} else {
    & $compiler @compilerArgs
}

if ($LASTEXITCODE -ne 0) {
    throw "Biên dịch test thất bại với mã $LASTEXITCODE"
}

& $testExe
if ($LASTEXITCODE -ne 0) {
    throw "Normalization tests thất bại với mã $LASTEXITCODE"
}

if ($Coverage) {
    $gcov = Resolve-TestTool "gcov.exe" ($mingwBinFallbacks | ForEach-Object { Join-Path $_ "gcov.exe" })
    Push-Location $outputDirectory
    try {
        $gcovOutput = (& $gcov -b -c -r ".\normalization_coverage-text_normalization.gcno" 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0) {
            throw "gcov thất bại với mã $LASTEXITCODE"
        }
        Write-Host $gcovOutput

        $lineCoverage = [regex]::Match($gcovOutput, "Lines executed:(\d+(?:\.\d+)?)%")
        if (-not $lineCoverage.Success) {
            throw "Không đọc được line coverage của text_normalization.cpp"
        }
        if ([double]$lineCoverage.Groups[1].Value -lt 80.0) {
            throw "Line coverage thấp hơn 80%: $($lineCoverage.Groups[1].Value)%"
        }
    } finally {
        Pop-Location
    }
}
