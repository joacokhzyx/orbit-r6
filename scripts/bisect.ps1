# Verdict on one commit, for `git bisect run`.
#   0   good: driver builds, run_reject.py, run_diff.py and the self-host pass
#   1   bad: one of the three checks failed
#   125 skip: the driver does not compile at this commit
#   128 abort: the environment is broken (missing runtime archive, no checkout)
# Older commits do not carry this script or scripts/selfhost.ps1. Copy both to
# a directory outside the tree first; -Repo is the tree git bisect checks out
# (default: the current directory, which is where `git bisect run` starts).
#
#   Copy-Item scripts\bisect.ps1, scripts\selfhost.ps1 C:\Temp\r6-bisect\
#   git bisect run pwsh -NoProfile -File C:\Temp\r6-bisect\bisect.ps1 `
#       -Compiler C:\Temp\orbit-fp-ee60c8af43.exe -OrbitRoot C:\Temp\orbit-pin-01e31e8-git `
#       -Cc C:\w64devkit\bin\gcc.exe
#
# The differential reads .r6-build\oracle-cache.json when the commit's
# run_diff.py has the cache, so only the first commit pays for the oracles.
param(
    [Parameter(Mandatory = $true)][string]$Compiler,
    [Parameter(Mandatory = $true)][string]$OrbitRoot,
    [Parameter(Mandatory = $true)][string]$Cc,
    [string]$Repo = (Get-Location).Path,
    [switch]$NoSelfhost
)
$ErrorActionPreference = 'Continue'
$Repo = (git -C $Repo rev-parse --show-toplevel)
if ($LASTEXITCODE -ne 0 -or -not $Repo) { Write-Host 'bisect: not inside a git checkout'; exit 128 }
$Repo = (Resolve-Path $Repo).Path
$here = Get-Location
Set-Location $Repo
try {
    $head = (git rev-parse --short HEAD)
    $archive = Join-Path $Repo '.r6-build\runtime-win64\libruntime_net.a'
    if (-not (Test-Path $archive)) { Write-Host "bisect ${head}: missing $archive"; exit 128 }

    $r6 = Join-Path $Repo '.r6-build\r6.exe'
    Remove-Item -Force $r6 -ErrorAction SilentlyContinue
    $global:LASTEXITCODE = 0
    try { & (Join-Path $Repo 'scripts\build-r6.ps1') | Out-Null } catch { $global:LASTEXITCODE = 1 }
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $r6)) { Write-Host "bisect ${head}: SKIP (driver does not compile)"; exit 125 }

    python tests/reject/run_reject.py | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { Write-Host "bisect ${head}: BAD (run_reject.py)"; exit 1 }

    python tests/diff/run_diff.py --compiler $Compiler --cc $Cc --orbit-root $OrbitRoot `
        --r6-command $r6 build '{ir}' -o '{exe}' | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { Write-Host "bisect ${head}: BAD (run_diff.py)"; exit 1 }

    if (-not $NoSelfhost) {
        & (Join-Path $PSScriptRoot 'selfhost.ps1') -OrbitRoot $OrbitRoot -Compiler $Compiler -R6 $r6 -Repo $Repo |
            Select-Object -Last 1
        if ($LASTEXITCODE -ne 0) { Write-Host "bisect ${head}: BAD (self-host)"; exit 1 }
    }
    Write-Host "bisect ${head}: GOOD"
    exit 0
} finally {
    Set-Location $here
}
