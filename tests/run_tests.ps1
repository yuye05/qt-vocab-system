param(
    [string]$QtBin,
    [string]$CompilerBin
)

$ErrorActionPreference = 'Stop'
if (-not $QtBin) { $QtBin = Split-Path (Get-Command qmake -ErrorAction Stop).Source }
if (-not $CompilerBin) { $CompilerBin = Split-Path (Get-Command g++ -ErrorAction Stop).Source }
$env:PATH = "$CompilerBin;$QtBin;" + $env:PATH
$qmakePath = Join-Path $QtBin 'qmake.exe'
$makePath = Join-Path $CompilerBin 'mingw32-make.exe'
$repoPath = Split-Path $PSScriptRoot
$buildRoot = Join-Path $env:TEMP ('qt-vocab-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildRoot -ErrorAction Stop | Out-Null

function Run-Test([string]$name, [string]$project, [string]$source, [string]$executable) {
    $testBuild = Join-Path $buildRoot $name
    New-Item -ItemType Directory -Path $testBuild -ErrorAction Stop | Out-Null
    if ($name -eq 'gui-smoke') {
        New-Item -ItemType Directory -Path (Join-Path $testBuild 'words') -ErrorAction Stop | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoPath 'words/dictionary.txt'), (Join-Path $repoPath 'words/wrong_words.txt') `
            -Destination (Join-Path $testBuild 'words') -ErrorAction Stop
    }
    Push-Location -LiteralPath $testBuild
    try {
        $testSource = (Join-Path $PSScriptRoot $source).Replace('\', '/')
        & $qmakePath (Join-Path $PSScriptRoot $project) 'CONFIG+=release' 'CONFIG-=debug_and_release' 'DESTDIR=.' `
            "TEST_SOURCE=$testSource"
        if ($LASTEXITCODE -ne 0) { throw "$name qmake failed" }
        & $makePath -j4 *> build.log
        if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath build.log -Tail 30; throw "$name compilation failed" }
        & (Join-Path $testBuild $executable)
        if ($LASTEXITCODE -ne 0) { throw "$name regression failed" }
    } finally { Pop-Location }
}

$previousPlatform = $env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM = 'offscreen'
    Run-Test 'core-loading' 'core_tests.pro' 'dictionary_loading_test.cpp' 'core_tests.exe'
    Run-Test 'core-safety' 'core_tests.pro' 'dictionary_safety_test.cpp' 'core_tests.exe'
    Run-Test 'gui-smoke' 'gui_loading_smoke.pro' 'gui_loading_smoke.cpp' 'gui_loading_smoke.exe'
    Run-Test 'gui-regression' 'gui_loading_smoke.pro' 'functional_regression.cpp' 'gui_loading_smoke.exe'
    Write-Output "PASS: all four regression programs. Build logs: $buildRoot"
} finally { $env:QT_QPA_PLATFORM = $previousPlatform }
