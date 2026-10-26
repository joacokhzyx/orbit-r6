# Entry point of the continuous integration. Stops at the first failing stage.
#   Stage 1 (always): driver build, make_cases.py --check, run_reject.py.
#   -Diff:   tests/diff/run_diff.py   (needs -Compiler, -OrbitRoot, -Cc)
#   -Corpus: tests/corpus/run_corpus.py --check (same inputs)
# Stage 1 needs .r6-build\runtime-win64\libruntime_net.a (see docs/PIN.md).
param(
    [switch]$Diff,
    [switch]$Corpus,
    [string]$Compiler,
    [string]$OrbitRoot,
    [string]$Cc
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

function Invoke-Stage([string]$Name, [scriptblock]$Body) {
    Write-Host "== $Name"
    $global:LASTEXITCODE = 0
    & $Body
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Host "FAILED: $Name (exit $code)"
        exit $code
    }
}

$archive = Join-Path $root '.r6-build\runtime-win64\libruntime_net.a'
if (-not (Test-Path $archive)) {
    Write-Host "FAILED: missing $archive (rebuild it with scripts/mklib_runtime.py from the pinned checkout; see docs/PIN.md)"
    exit 1
}
if (($Diff -or $Corpus) -and -not ($Compiler -and $OrbitRoot -and $Cc)) {
    Write-Host 'FAILED: -Diff and -Corpus need -Compiler, -OrbitRoot and -Cc'
    exit 2
}

Invoke-Stage 'driver build' { & (Join-Path $PSScriptRoot 'build-r6.ps1') }
Invoke-Stage 'reject cases drift' { python tests/reject/make_cases.py --check }
Invoke-Stage 'loader suite' { python tests/reject/run_reject.py }

if ($Diff) {
    Invoke-Stage 'differential' {
        python tests/diff/run_diff.py --compiler $Compiler --cc $Cc --orbit-root $OrbitRoot `
            --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
    }
}
if ($Corpus) {
    Invoke-Stage 'corpus ratchet' {
        python tests/corpus/run_corpus.py --compiler $Compiler --cc $Cc --orbit-root $OrbitRoot --check `
            --r6-command .r6-build\r6.exe build '{ir}' -o '{exe}'
    }
}
Write-Host 'CI OK'
