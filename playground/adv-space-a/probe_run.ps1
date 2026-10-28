$ErrorActionPreference = 'Stop'
$lane = Split-Path -Parent $PSCommandPath
$repo = Split-Path -Parent (Split-Path -Parent $lane)
$out = Join-Path $lane 'out'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Host "[probe_run] enter lane=$lane"
$envJson = Get-Content (Join-Path $repo 'build/env.json') -Raw | ConvertFrom-Json
$clang = $envJson.clang.path
$python = $envJson.python.path
Write-Host "[probe_run] clang=$clang"
$src = Join-Path $repo 'src'
$commonFlags = @('-target', 'x86_64-pc-windows-msvc', '-std=c23', '-Wall', '-Wextra',
    '-ffp-contract=off', '-g0', '-D_CRT_SECURE_NO_WARNINGS', '-O2', '-DNDEBUG', '-I', $src)
$linkFlags = @('-fuse-ld=lld-link', '-Xlinker', '/SUBSYSTEM:CONSOLE', '-Xlinker', '/brepro')
$oldDefs = @('-Dbt_alg_init=old_alg_init', '-Dbt_alg_free=old_alg_free',
    '-Dbt_alg_push=old_alg_push', '-Dbt_alg_len=old_alg_len',
    '-Dbt_alg_is_empty=old_alg_is_empty', '-Dbt_alg_eq=old_alg_eq',
    '-Dbt_alg_parse_token=old_alg_parse_token', '-Dbt_alg_parse=old_alg_parse',
    '-Dbt_alg_format=old_alg_format', '-Dbt_alg_try_parse=old_alg_try_parse',
    '-Dbt_alg_inverse=old_alg_inverse', '-Dbt_alg_qtm=old_alg_qtm',
    '-Dbt_alg_htm=old_alg_htm')

function Invoke-Build {
    param([string]$Name, [array]$Units)
    $objs = @()
    $idx = 0
    foreach ($unit in $Units) {
        $obj = Join-Path $out ("{0}_{1}.obj" -f $Name, $idx)
        $defs = @()
        if ($null -ne $unit.Defs) {
            $defs = $unit.Defs
        }
        & $clang @commonFlags @defs -c $unit.Source -o $obj
        if ($LASTEXITCODE -ne 0) {
            throw "[probe_run] compile failed unit $idx of $Name"
        }
        $objs += $obj
        $idx++
    }
    $exe = Join-Path $out "$Name.exe"
    & $clang @commonFlags @linkFlags @objs -o $exe
    if ($LASTEXITCODE -ne 0) {
        throw "[probe_run] link failed for $Name"
    }
    Write-Host "[probe_run] built $Name"
}

function Invoke-Probe {
    param([string]$Name)
    $exe = Join-Path $out "$Name.exe"
    $log = Join-Path $out "$Name.log"
    $output = & $exe 2>&1
    $code = $LASTEXITCODE
    $output | Set-Content -Path $log
    foreach ($line in ($output | Select-Object -Last 15)) {
        Write-Host $line
    }
    if ($code -ne 0) {
        Write-Host "[probe_run] FAIL $Name exit=$code"
        return 1
    }
    Write-Host "[probe_run] pass $Name"
    return 0
}

& $python (Join-Path $lane 'extract_old.py')
if ($LASTEXITCODE -ne 0) {
    throw "[probe_run] extract_old failed"
}

Invoke-Build 'probe_equiv' @(
    @{ Source = (Join-Path $lane 'probe_equiv.c') },
    @{ Source = (Join-Path $lane 'old_ws.c') },
    @{ Source = (Join-Path $src 'common/space.c') },
    @{ Source = (Join-Path $src 'common/bt_trace.c') }
)

Invoke-Build 'probe_parse' @(
    @{ Source = (Join-Path $lane 'probe_parse.c') },
    @{ Source = (Join-Path $lane 'old_alg.c'); Defs = $oldDefs },
    @{ Source = (Join-Path $src 'core/alg.c') },
    @{ Source = (Join-Path $src 'common/space.c') },
    @{ Source = (Join-Path $src 'common/bt_trace.c') },
    @{ Source = (Join-Path $src 'common/strbuf.c') },
    @{ Source = (Join-Path $src 'core/moves.c') }
)

Invoke-Build 'alg_tests' @(
    @{ Source = (Join-Path $repo 'tests/unit/core/alg_test.c') },
    @{ Source = (Join-Path $lane 'run_alg_tests.c') },
    @{ Source = (Join-Path $src 'common/bt_harness.c') },
    @{ Source = (Join-Path $src 'core/alg.c') },
    @{ Source = (Join-Path $src 'common/space.c') },
    @{ Source = (Join-Path $src 'common/bt_trace.c') },
    @{ Source = (Join-Path $src 'common/strbuf.c') },
    @{ Source = (Join-Path $src 'core/moves.c') }
)

$fail = 0
$fail += Invoke-Probe 'alg_tests'
$fail += Invoke-Probe 'probe_equiv'
$fail += Invoke-Probe 'probe_parse'
Write-Host "[probe_run] exit failures=$fail"
exit $fail
