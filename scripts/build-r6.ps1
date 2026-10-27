$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root
$build = Join-Path $root '.r6-build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$cc = if ($env:R6_CC) { $env:R6_CC } elseif ($env:CC) { $env:CC } else { 'gcc' }
$exe = Join-Path $build 'r6.exe'
& $cc -std=c11 -Wall -Wextra -Werror `
    src/driver/r6.c src/loader/ir.c src/codegen-x64/win64.c `
    src/codegen-x64/asm_win64.c src/codegen-x64/coff.c src/codegen-x64/peephole.c `
    -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output $exe
