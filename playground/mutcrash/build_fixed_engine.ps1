$ErrorActionPreference = 'Stop'
$repo = 'C:\Users\lavantien\dev\github\blind-trainer'
$out = Join-Path $repo 'playground\mutcrash\fixed-engine'
New-Item -ItemType Directory -Force $out | Out-Null
$flags = @('-target', 'x86_64-pc-windows-msvc', '-std=c23', '-Wall', '-Wextra', '-Werror',
    '-ffp-contract=off', '-g0', '-D_CRT_SECURE_NO_WARNINGS', '-O2', '-DNDEBUG',
    "-I$repo\tools", "-I$repo\src", "-I$repo\tests")
$clang = (Get-Content (Join-Path $repo 'build\env.json') -Raw | ConvertFrom-Json).clang.path
$objs = @()
foreach ($name in @('buildcmd', 'engine', 'fs', 'ledger', 'main', 'operators', 'report',
        'scratch', 'spawn', 'tests', 'tests_ops', 'token', 'union')) {
    $o = Join-Path $out "$name.obj"
    & $clang @flags -c (Join-Path $repo "tools\mutate\$name.c") -o $o
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $name" }
    $objs += $o
}
$hashDir = Get-ChildItem (Join-Path $repo 'build\obj') -Directory | Select-Object -First 1
$common = Get-ChildItem (Join-Path $hashDir.FullName 'src\common\*.obj') | ForEach-Object { $_.FullName }
$linkFlags = @('-fuse-ld=lld-link', '-Xlinker', '/SUBSYSTEM:CONSOLE', '-Xlinker', '/OPT:REF',
    '-Xlinker', '/OPT:ICF', '-Xlinker', '/DEBUG:NONE', '-Xlinker', '/brepro')
$exe = Join-Path $repo 'playground\mutcrash\mutate-fixed.exe'
& $clang @flags @linkFlags @objs @common -o $exe
if ($LASTEXITCODE -ne 0) { throw 'link failed' }
Write-Output "built $exe"
