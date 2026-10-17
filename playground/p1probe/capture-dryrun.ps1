param([Parameter(Mandatory = $true)][string]$OutPath)

Set-Location (Join-Path $PSScriptRoot '..\..')

$targets = @(
  'setup', 'docs-fetch', 'gen-data', 'verify-algs', 'verify-algs-selftest', 'synth-simcheck',
  'memofreq', 'ga', 'fmt', 'fmt-fix', 'lint', 'check', 'build', 'jobwrap', 'test',
  'policy', 'policy-selftest', 'mutants-gate', 'mutants-gate-selftest', 'fuzz', 'mutation',
  'mutation-merge', 'e2e', 'e2e-smoke', 'visual', 'visual-diff', 'visual-selftest', 'baselines',
  'screenshots', 'memguard', 'memguard-selftest', 'clean-rust', 'clean-rust-apply',
  'clean-rust-deep-apply', 'clean-rust-selftest', 'run', 'chain', 'coverage', 'alg-coverage',
  'diagrams', 'release-verify-selftest', 'clean'
)

$parameterized = @(
  @{ t = 'test-crate';    a = 'p=bt-core' },
  @{ t = 'mutation-pkg';  a = 'p=bt-core' },
  @{ t = 'release-verify'; a = 'v=v0.0.0-probe' },
  @{ t = 'dist';          a = 'v=v0.0.0-probe' },
  @{ t = 'release';       a = 'v=v0.0.0-probe' }
)

Remove-Item -Path $OutPath -ErrorAction SilentlyContinue

foreach ($t in $targets) {
  "=== make -n $t ===" | Add-Content -Path $OutPath
  make -n $t 2>&1 | Add-Content -Path $OutPath
  "exit=$LASTEXITCODE" | Add-Content -Path $OutPath
}

foreach ($e in $parameterized) {
  "=== make -n $($e.t) $($e.a) ===" | Add-Content -Path $OutPath
  make -n $e.t $e.a 2>&1 | Add-Content -Path $OutPath
  "exit=$LASTEXITCODE" | Add-Content -Path $OutPath
}

'done' | Add-Content -Path $OutPath
