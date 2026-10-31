# adv78b progress log

adversary b review of c0cd544 + eb767a6 (self-hosted ci badge).

## reading log

read scripts/ci_badge.py (full, via git show c0cd544) - the change under attack
read tests/python/test_ci_badge.py (full, via git show c0cd544) - landed coverage claims
read scripts/badgesvg.py (full, 33 lines) - color override and xml interpolation
read scripts/slog.py (full, 31 lines) - TOOL fail and exit semantics
read .github/workflows/ci.yml (lines 1 to 54) - automation wiring check
read README.md diff of eb767a6 plus worktree drift - consumer wiring

## probe runs

2026-10-31 23:38 built playground/adv78b/probe_cibadge.py, 20 cases: stubbed gh payloads (exact live run, other run, failed, timed_out, empty, truncated, non-list, invalid utf-8, unicode extras, 50-entry arrays both orders, null ids, string id, auth failure, 60s hang) plus live gh (current run, limit-5 ordering, zero-run branch, stripped-auth) and xml validation of every verdict svg.
2026-10-31 23:40 first no-gh probe invalid: pwsh 7 Start-Process -Environment merges PATH with the inherited one instead of replacing it, so gh stayed reachable. rediagnosed with diag_which.py, reran clean through python subprocess env replacement in probe_no_gh.py.
2026-10-31 23:42 ran the landed test module: 18 tests, all ok.
2026-10-31 23:44 hygiene scans: 0 comments in all 4 touched files, sloc 102/153/29/10. raw github serves docs/badges/coverage.svg as image/svg+xml 200, repo public.

## verdict

findings reported to team-lead: 1 confirmed staleness (no automation refreshes the badge), 1 confirmed verdict gap (timed_out renders grey unknown), 1 minor stdout detail misreport on the offline path, 2 held (force-push stale tip, workflow-name overlap). everything else attacked held: honest-unbacked behavior in every degraded case, determinism, parse robustness, consumer rendering, hygiene.
