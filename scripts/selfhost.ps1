# Self-host check: R6 builds the pinned compiler, and that compiler reproduces
# the published fixed-point C.
#   1. The fixed point emits the IR of compiler/host/main.orb (ORBIT_SKIP_CC=1).
#   2. R6 builds that IR into orbitc.exe.
#   3. orbitc.exe compiles compiler/host/main.orb again; its generated C must
#      hash to PUBLISHED_C in the pin's scripts/verify_seed.py.
# Exit 0 = SELFHOST OK, 1 = anything else. Work files go to <Repo>\.r6-build\selfhost.
# -Repo defaults to this script's checkout; scripts/bisect.ps1 passes the tree
# under test, because a copy of this script outside the tree runs there.
param(
    [Parameter(Mandatory = $true)][string]$OrbitRoot,
    [Parameter(Mandatory = $true)][string]$Compiler,
    [Parameter(Mandatory = $true)][string]$R6,
    [string]$Repo = (Join-Path $PSScriptRoot '..')
)
$ErrorActionPreference = 'Continue'
$root = (Resolve-Path $Repo).Path
$OrbitRoot = (Resolve-Path $OrbitRoot).Path
$Compiler = (Resolve-Path $Compiler).Path
$R6 = (Resolve-Path $R6).Path

function Fail([string]$Why) {
    Write-Host "SELFHOST FAIL: $Why"
    exit 1
}

$commit = (git -C $OrbitRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0) { Fail "$OrbitRoot is not a git checkout" }
$seed = (git -C $OrbitRoot cat-file blob "${commit}:scripts/verify_seed.py") -join "`n"
if ($LASTEXITCODE -ne 0) { Fail "no scripts/verify_seed.py at $commit" }
$match = [regex]::Match($seed, '(?m)^PUBLISHED_C = "([0-9A-Fa-f]{64})"')
if (-not $match.Success) { Fail "no PUBLISHED_C in scripts/verify_seed.py at $commit" }
$expected = $match.Groups[1].Value.ToUpperInvariant()
Write-Host "pin $commit, PUBLISHED_C $expected"

$work = Join-Path $root '.r6-build\selfhost'
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
$run = Join-Path $work 'run'
New-Item -ItemType Directory -Force $run | Out-Null
$saved = @{ TEMP = $env:TEMP; TMP = $env:TMP; ORBIT_CCACHE = $env:ORBIT_CCACHE; ORBIT_SKIP_CC = $env:ORBIT_SKIP_CC }
$ir = Join-Path $work 'main.ir'
$orbitc = Join-Path $work 'orbitc.exe'
$here = Get-Location
try {
    $env:ORBIT_CCACHE = '0'
    $env:ORBIT_SKIP_CC = '1'
    $env:TEMP = $work; $env:TMP = $work
    Push-Location $OrbitRoot
    $null = & $Compiler build compiler/host/main.orb -o (Join-Path $work 'fp.exe') --emit-ir $ir 2>&1
    Pop-Location
    if (-not (Test-Path $ir)) { Fail "the fixed point emitted no IR (exit $LASTEXITCODE)" }
    Write-Host "ir: $((Get-Item $ir).Length) bytes"

    Remove-Item Env:ORBIT_SKIP_CC
    Push-Location $root
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $diag = & $R6 build $ir -o $orbitc 2>&1
    $code = $LASTEXITCODE
    Pop-Location
    Write-Host ("r6 build: {0:N1} s, exit {1}" -f $sw.Elapsed.TotalSeconds, $code)
    if ($code -ne 0 -or -not (Test-Path $orbitc)) { Fail "R6 did not build the compiler: $($diag | Select-Object -First 3)" }

    $env:ORBIT_SKIP_CC = '1'
    $env:TEMP = $run; $env:TMP = $run
    Push-Location $OrbitRoot
    $null = & $orbitc build compiler/host/main.orb -o (Join-Path $run 'x.exe') 2>&1
    Pop-Location
    $c = Join-Path $run 'orbit_selfhost_build.c'
    if (-not (Test-Path $c)) { Fail "orbitc.exe wrote no orbit_selfhost_build.c (exit $LASTEXITCODE)" }
    $got = (Get-FileHash $c -Algorithm SHA256).Hash
    Write-Host "C sha256: $got"
    if ($got -ne $expected) { Fail "expected $expected" }
    Write-Host 'SELFHOST OK'
    exit 0
} finally {
    Set-Location $here
    foreach ($name in $saved.Keys) {
        if ($null -eq $saved[$name]) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item "Env:$name" $saved[$name] }
    }
}
