[CmdletBinding()]
param(
    [string]$QtRoot = 'C:/Qt/6.11.2/mingw_64',
    [string]$MingwRoot = 'C:/Qt/Tools/mingw1310_64',
    [string]$CMakeExe = 'C:/Qt/Tools/CMake_64/bin/cmake.exe',
    [string]$BuildDirectory = 'cmake-build-w10-release',
    [string]$OutputDirectory = 'release-candidate/app',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-LastCommand([string]$Description) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

$buildPath = Resolve-ProjectPath $BuildDirectory
$outputPath = Resolve-ProjectPath $OutputDirectory
$releaseRoot = Resolve-ProjectPath 'release-candidate'
$releasePrefix = $releaseRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $outputPath.StartsWith($releasePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDirectory must be a child of release-candidate.'
}

$compiler = Join-Path $MingwRoot 'bin/g++.exe'
$make = Join-Path $MingwRoot 'bin/mingw32-make.exe'
$deploy = Join-Path $QtRoot 'bin/windeployqt.exe'
$requiredTools = @($CMakeExe, $compiler, $make, $deploy)
foreach ($tool in $requiredTools) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required tool not found: $tool"
    }
}

if (Test-Path -LiteralPath $outputPath) {
    if (-not $Force) {
        throw "Output already exists: $outputPath. Re-run with -Force to replace it."
    }
    Remove-Item -LiteralPath $outputPath -Recurse -Force
}
New-Item -ItemType Directory -Path $outputPath | Out-Null

$savedPath = $env:PATH
$savedPlatform = $env:QT_QPA_PLATFORM
try {
    $env:PATH = "$(Join-Path $MingwRoot 'bin');$(Join-Path $QtRoot 'bin');$savedPath"

    & $CMakeExe -S $projectRoot -B $buildPath -G 'MinGW Makefiles' `
        -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtRoot" `
        "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_MAKE_PROGRAM=$make" `
        -DBUILD_TESTING=ON
    Assert-LastCommand 'CMake configure'

    & $CMakeExe --build $buildPath --parallel 4
    Assert-LastCommand 'Release build'

    & $CMakeExe -E env "PATH=$env:PATH" `
        (Join-Path (Split-Path -Parent $CMakeExe) 'ctest.exe') `
        --test-dir $buildPath --output-on-failure
    Assert-LastCommand 'Release CTest'

    $builtExe = Join-Path $buildPath 'TakeoutOrderManagementSystem.exe'
    if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf)) {
        throw "Built executable not found: $builtExe"
    }
    $packagedExe = Join-Path $outputPath 'TakeoutOrderManagementSystem.exe'
    Copy-Item -LiteralPath $builtExe -Destination $packagedExe

    & $deploy --release --compiler-runtime --no-translations `
        --dir $outputPath $packagedExe
    Assert-LastCommand 'windeployqt'

    foreach ($runtime in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll',
                            'libwinpthread-1.dll')) {
        $source = Join-Path (Join-Path $MingwRoot 'bin') $runtime
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "MinGW runtime not found: $source"
        }
        Copy-Item -LiteralPath $source -Destination $outputPath -Force
    }

    $requiredPackageFiles = @(
        'TakeoutOrderManagementSystem.exe',
        'Qt6Core.dll',
        'Qt6Gui.dll',
        'Qt6Widgets.dll',
        'Qt6Network.dll',
        'libgcc_s_seh-1.dll',
        'libstdc++-6.dll',
        'libwinpthread-1.dll',
        'platforms/qwindows.dll'
    )
    foreach ($relativePath in $requiredPackageFiles) {
        $file = Join-Path $outputPath $relativePath
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Packaged dependency missing: $relativePath"
        }
    }

    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = 'windows'
    & $packagedExe --smoke-test
    Assert-LastCommand 'Packaged smoke test'

    Write-Host "Release package verified: $outputPath"
}
finally {
    $env:PATH = $savedPath
    $env:QT_QPA_PLATFORM = $savedPlatform
}
