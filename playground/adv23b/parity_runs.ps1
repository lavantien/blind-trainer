$ErrorActionPreference = 'Continue'
python bt.py ga-parity *> playground/adv23b/ga-parity-run1.log
"exit=$LASTEXITCODE" | Add-Content playground/adv23b/ga-parity-run1.log
Copy-Item artifacts/parity/verdict.json playground/adv23b/verdict-run1.json -Force
python bt.py ga-parity *> playground/adv23b/ga-parity-run2.log
"exit=$LASTEXITCODE" | Add-Content playground/adv23b/ga-parity-run2.log
Copy-Item artifacts/parity/verdict.json playground/adv23b/verdict-run2.json -Force
$r1 = (Get-FileHash playground/adv23b/verdict-run1.json -Algorithm SHA256).Hash
$r2 = (Get-FileHash playground/adv23b/verdict-run2.json -Algorithm SHA256).Hash
$rc = (Get-FileHash playground/adv23b/verdict-committed.json -Algorithm SHA256).Hash
$rw = (Get-FileHash artifacts/parity/verdict.json -Algorithm SHA256).Hash
"run1=$r1"
"run2=$r2"
"committed=$rc"
"worktree_after=$rw"
"run1_eq_run2=$($r1 -eq $r2)"
"fresh_eq_committed=$($rw -eq $rc)"
