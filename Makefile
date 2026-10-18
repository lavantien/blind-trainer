SHELL := pwsh.exe
.SHELLFLAGS := -NoProfile -ExecutionPolicy Bypass -Command

REF_GROUND := ref_ground
CONFIG := config/config.json
ARTIFACTS := artifacts
BIN := target/release/blind-trainer.exe
E2E_BIN := tests/e2e/target/release/bt-e2e.exe
MEMGUARD := scripts/memguard.ps1
JOBWRAP := target/release/bt-jobwrap.exe
MAKE_CMD := $(MAKE)
FUZZ_TARGETS := notation classify memo commutator expand db fsrs
CARGO_PROXY ?= C:/Users/lavantien/.cargo/bin/cargo.exe
ASAN_RT_GLOB ?= C:/Program Files/Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64
MUTATION_PACKAGES := bt-core bt-scheme bt-data bt-ga bt-puzzles

export BT_REF_GROUND := $(REF_GROUND)
export BT_CONFIG := $(CONFIG)
export BT_ARTIFACTS := $(ARTIFACTS)

.PHONY: help setup docs-fetch gen-data verify-algs verify-algs-selftest synth-simcheck memofreq ga fmt fmt-fix lint check build jobwrap test test-crate policy policy-selftest mutants-gate mutants-gate-selftest fuzz mutation mutation-pkg mutation-merge e2e e2e-smoke visual visual-diff visual-selftest baselines screenshots memguard memguard-selftest clean-rust clean-rust-apply clean-rust-deep-apply clean-rust-selftest run chain coverage alg-coverage diagrams dist release-verify release-verify-selftest release clean

COVERAGE_TARGET ?= C:/Users/lavantien/dev/github/bt-coverage-target
MUTATION_TARGET ?= C:/Users/lavantien/dev/github/bt-mutation-target
STALE_HOURS ?= -1

OUTCOMES ?= mutants.out/outcomes.json
ALLOWLIST ?= tools/bt-mutants-gate/allowlist.tsv
MUTATION_PKG_DIR ?= mutants.out/pkg
MUTATION_PKG_OUTS := $(foreach pkg,$(MUTATION_PACKAGES),$(MUTATION_PKG_DIR)/$(pkg)/mutants.out/outcomes.json)

help: ## show this help
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/help.ps1

setup: ## ensure rustfmt and clippy, report cargo-fuzz and cargo-mutants
	rustup component add rustfmt clippy
	@if (Get-Command cargo-fuzz -ErrorAction SilentlyContinue) { Write-Output 'cargo-fuzz present' } else { Write-Output 'cargo-fuzz missing' }
	@if (Get-Command cargo-mutants -ErrorAction SilentlyContinue) { Write-Output 'cargo-mutants present' } else { Write-Output 'cargo-mutants missing' }

docs-fetch: ## snapshot reference docs into ref_doc/
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/fetch_docs.ps1

gen-data: ## build data/vendor from ref_ground and synthesize uncovered pairs
	cargo run --release -p bt-gendata

verify-algs: ## verify every vendor alg and pack row, write the report
	cargo run --release -p bt-verify

verify-algs-selftest: ## the bt-verify unit suite with planted offenders
	cargo test -p bt-verify

synth-simcheck: ## run the independent pysim simulator over synthesized algs
	python -I scripts/synth_simcheck.py

memofreq: ## memo pair frequency monte carlo
	cargo run --release -p bt-memofreq

ga: ## run the syllable optimizer into data/ga/
	cargo run --release -p bt-ga-run

fmt: ## rustfmt check over the workspace
	cargo fmt --all -- --check

fmt-fix: ## rustfmt in place
	cargo fmt --all

lint: ## clippy -D warnings under jobwrap
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor lint -- cargo clippy --workspace --all-targets -- -D warnings

policy: ## policy selftest then the zero-comment and constants-hub scan
	$(MAKE) policy-selftest
	cargo run --release -p bt-policy

policy-selftest: ## prove bt-policy detects comments and constants
	cargo run --release -p bt-policy -- selftest $(ARTIFACTS)/policy-selftest

check: ## cargo check the workspace
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor check -- cargo check --workspace

build: ## release build of blind-trainer.exe
	cargo build --release -p bt-app
	if (-not (Test-Path '$(BIN)')) { Write-Output 'missing $(BIN)'; exit 1 }

jobwrap: ## build the jobwrap memory anchor exe
	cargo build --release -p bt-jobwrap
	if (-not (Test-Path '$(JOBWRAP)')) { Write-Output 'missing $(JOBWRAP)'; exit 1 }

test: ## workspace test suite under jobwrap
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor test -- cargo test --workspace

test-crate: ## one crate's tests: make test-crate p=<crate>
	@if ([string]::IsNullOrEmpty('$(p)')) { Write-Output 'usage: make test-crate p=<crate>'; exit 2 }
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor test-$(p) -- cargo test -p $(p)

mutants-gate: ## adjudicate mutation survivors against the allowlist
	cargo run --release -p bt-mutants-gate -- $(OUTCOMES) $(ALLOWLIST)

mutants-gate-selftest: ## the bt-mutants-gate unit suite
	cargo test -p bt-mutants-gate

fuzz: ## run every cargo-fuzz target under asan
	$(MAKE_CMD) jobwrap
	$$cp = Split-Path -Parent '$(CARGO_PROXY)'; $$asan = (Get-ChildItem '$(ASAN_RT_GLOB)' -Directory | Where-Object { Test-Path (Join-Path $$_.FullName 'clang_rt.asan_dynamic-x86_64.dll') } | Sort-Object FullName -Descending | Select-Object -First 1).FullName; $$env:PATH = "$$cp;$$asan;" + $$env:PATH; Set-Location fuzz; foreach ($$t in '$(FUZZ_TARGETS)'.Split(' ')) { $(CURDIR)/$(JOBWRAP) --anchor fuzz-$$t -- cargo fuzz run $$t; if ($$LASTEXITCODE -ne 0) { exit 1 } }

mutation: ## cargo mutants jobs 1 over the mutated crates
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor mutation -- cargo mutants --jobs 1 $(addprefix -p ,$(MUTATION_PACKAGES))

mutation-pkg: ## one crate's mutants: make mutation-pkg p=<crate>
	@if ([string]::IsNullOrEmpty('$(p)')) { Write-Output 'usage: make mutation-pkg p=<crate>'; exit 2 }
	$(MAKE_CMD) jobwrap
	New-Item -Path '$(MUTATION_PKG_DIR)/$(p)' -ItemType Directory -Force | Out-Null
	$(JOBWRAP) --anchor mutation-pkg-$(p) -- cargo mutants --jobs 1 -p $(p) -o $(MUTATION_PKG_DIR)/$(p)

mutation-merge: ## merge per-package outcomes for the gate
	cargo run --release -p bt-mutants-gate -- merge $(OUTCOMES) $(MUTATION_PKG_OUTS)

e2e: build ## headless flow suite then the smoke tour
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor e2e -- cargo test --release -p bt-app --test e2e
	$(MAKE_CMD) e2e-smoke

e2e-smoke: build ## sandboxed smoke tour over the release exe
	cargo build --release --manifest-path tests/e2e/Cargo.toml
	New-Item -Path '$(ARTIFACTS)/e2e', '$(ARTIFACTS)/e2e-user' -ItemType Directory -Force | Out-Null
	$$env:APPDATA = '$(ARTIFACTS)/e2e-user'; $(E2E_BIN) $(BIN) $(ARTIFACTS)/e2e

visual: visual-selftest ## visual selftest then the diff with one re-render retry
	@$(MAKE) visual-diff; if ($$LASTEXITCODE -eq 0) { exit 0 }; Write-Output 'visual: mismatch, re-rendering and re-diffing once'; $(MAKE) e2e-smoke; if ($$LASTEXITCODE -ne 0) { Add-Content -Path '$(ARTIFACTS)/visual/report.md' -Value "`nverdict: infra error, re-render failed"; exit 4 }; $(MAKE) visual-diff; if ($$LASTEXITCODE -eq 0) { Add-Content -Path '$(ARTIFACTS)/visual/report.md' -Value "`nverdict: infra flake, second run passed"; exit 3 } else { Add-Content -Path '$(ARTIFACTS)/visual/report.md' -Value "`nverdict: regression, both runs mismatched"; exit 2 }

visual-diff: ## diff e2e shots against the committed baselines
	cargo run --release -p bt-visual-diff -- $(ARTIFACTS)/e2e tests/visual/baselines

visual-selftest: ## prove the diff passes identical and catches drift
	cargo run --release -p bt-visual-diff -- --selftest $(ARTIFACTS)/visual-selftest

baselines: ## copy e2e pngs into the committed baselines
	New-Item -Path tests/visual/baselines -ItemType Directory -Force | Out-Null
	Copy-Item -Path '$(ARTIFACTS)/e2e/*.png' -Destination tests/visual/baselines/ -ErrorAction Stop

screenshots: build ## regenerate every usage shot and the fatal screen
	if (Test-Path '$(ARTIFACTS)/usage-user') { Remove-Item -Path '$(ARTIFACTS)/usage-user' -Recurse -Force }
	Remove-Item -Path docs/screenshots/*.png -Force -ErrorAction SilentlyContinue
	New-Item -Path docs/screenshots -ItemType Directory -Force | Out-Null
	$$env:APPDATA = '$(ARTIFACTS)/usage-user'; $(BIN) --usage docs/screenshots
	$$env:BT_CONFIG = '$(ARTIFACTS)/usage-absent-config.json'; $(BIN) --fatal-shot docs/screenshots

memguard: ## report repo process memory against the cap
	pwsh -NoProfile -ExecutionPolicy Bypass -File $(MEMGUARD)

memguard-selftest: ## prove the memguard matcher with a sleeper
	cargo build --release -p bt-sleeper
	pwsh -NoProfile -ExecutionPolicy Bypass -File $(MEMGUARD) -SelfTest -SleeperPath target/release/bt-sleeper.exe

clean-rust: ## size regenerable caches against the disk floor, STALE_HOURS overrides the window
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -StaleHours $(STALE_HOURS) -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-apply: ## delete the sized caches
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -Apply -StaleHours $(STALE_HOURS) -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-deep-apply: ## deep clean the e2e and fuzz targets
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -Apply -Deep -StaleHours $(STALE_HOURS) -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-selftest: ## prove stale scratch selection
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -SelfTest -WorkDir $(ARTIFACTS)/clean-rust-selftest

run: ## run the app
	cargo run --release -p bt-app

chain: ## the full verification chain, one make call per step
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor chain -- $(MAKE_CMD) test fmt lint policy check build fuzz mutation mutants-gate e2e visual verify-algs

coverage: ## llvm-cov run and coverage badge refresh
	$$env:CARGO_TARGET_DIR = '$(COVERAGE_TARGET)'; pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/coverage-badge.ps1

alg-coverage: ## alg coverage badge refresh
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/alg-coverage-badge.ps1

diagrams: ## render every diagram png from its puml source
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/diagrams.ps1

release-verify: ## byte-check the dist zip and run the packaged exe
	@if ([string]::IsNullOrEmpty('$(v)')) { Write-Output 'usage: make release-verify v=<tag>'; exit 2 }
	cargo run --release -p bt-release-verify -- $(ARTIFACTS)/dist/blind-trainer $(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip --run

release-verify-selftest: ## the bt-release-verify unit suite
	cargo test -p bt-release-verify

dist: build ## build, stage and verify the dist zip: make dist v=<tag>
	@if ([string]::IsNullOrEmpty('$(v)')) { Write-Output 'usage: make dist v=<tag>'; exit 2 }
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/dist.ps1 -Version $(v)
	$(MAKE) release-verify v=$(v)

release: ## push, tag and publish the github release
	@if ([string]::IsNullOrEmpty('$(v)')) { Write-Output 'usage: make release v=<tag>'; exit 2 }
	@if (-not (Test-Path 'docs/releases/$(v).md')) { Write-Output 'missing docs/releases/$(v).md'; exit 2 }
	@if (-not (Test-Path '$(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip')) { Write-Output 'run make dist v=$(v) first'; exit 2 }
	git push origin main
	git push origin $(v)
	gh release create $(v) $(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip --title "blind trainer $(v)" --notes-file docs/releases/$(v).md

clean: ## remove artifacts and the e2e target
	Remove-Item -Path '$(ARTIFACTS)', tests/e2e/target -Recurse -Force -ErrorAction SilentlyContinue
	New-Item -Path '$(ARTIFACTS)' -ItemType Directory -Force | Out-Null
	if (-not (Test-Path '$(ARTIFACTS)/.gitkeep')) { New-Item -Path '$(ARTIFACTS)/.gitkeep' -ItemType File | Out-Null }
