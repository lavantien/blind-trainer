REF_GROUND := ref_ground
CONFIG := config/config.json
ARTIFACTS := artifacts
BIN := target/release/blind-trainer.exe
E2E_BIN := tests/e2e/target/release/bt-e2e.exe
MEMGUARD := scripts/memguard.ps1
JOBWRAP := target/release/bt-jobwrap.exe
MAKE_CMD := $(MAKE)
FUZZ_TARGETS := notation classify memo commutator expand db fsrs
MUTATION_PACKAGES := bt-core bt-scheme bt-data bt-ga bt-puzzles

export BT_REF_GROUND := $(REF_GROUND)
export BT_CONFIG := $(CONFIG)
export BT_ARTIFACTS := $(ARTIFACTS)

.PHONY: setup docs-fetch gen-data verify-algs verify-algs-selftest synth-simcheck memofreq ga fmt fmt-fix lint check build jobwrap test test-crate policy policy-selftest mutants-gate mutants-gate-selftest fuzz mutation mutation-pkg mutation-merge e2e e2e-smoke visual visual-diff visual-selftest baselines screenshots memguard memguard-selftest clean-rust clean-rust-apply clean-rust-deep-apply clean-rust-selftest run chain coverage alg-coverage diagrams dist release-verify release-verify-selftest release clean

COVERAGE_TARGET ?= C:/Users/lavantien/dev/github/bt-coverage-target
MUTATION_TARGET ?= C:/Users/lavantien/dev/github/bt-mutation-target

OUTCOMES ?= mutants.out/outcomes.json
ALLOWLIST ?= tools/bt-mutants-gate/allowlist.tsv
MUTATION_PKG_DIR ?= mutants.out/pkg
MUTATION_PKG_OUTS := $(foreach pkg,$(MUTATION_PACKAGES),$(MUTATION_PKG_DIR)/$(pkg)/mutants.out/outcomes.json)

setup:
	rustup component add rustfmt clippy
	@command -v cargo-fuzz >/dev/null 2>&1 && echo cargo-fuzz present || echo cargo-fuzz missing
	@command -v cargo-mutants >/dev/null 2>&1 && echo cargo-mutants present || echo cargo-mutants missing

docs-fetch:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/fetch_docs.ps1

gen-data:
	cargo run --release -p bt-gendata

verify-algs:
	cargo run --release -p bt-verify

verify-algs-selftest:
	cargo test -p bt-verify

synth-simcheck:
	python -I scripts/synth_simcheck.py

memofreq:
	cargo run --release -p bt-memofreq

ga:
	cargo run --release -p bt-ga-run

fmt:
	cargo fmt --all -- --check

fmt-fix:
	cargo fmt --all

lint:
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor lint -- cargo clippy --workspace --all-targets -- -D warnings

policy:
	$(MAKE) policy-selftest
	cargo run --release -p bt-policy

policy-selftest:
	cargo run --release -p bt-policy -- selftest $(ARTIFACTS)/policy-selftest

check:
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor check -- cargo check --workspace

build:
	cargo build --release -p bt-app
	test -f $(BIN) || { echo missing $(BIN); exit 1; }

jobwrap:
	cargo build --release -p bt-jobwrap
	test -f $(JOBWRAP) || { echo missing $(JOBWRAP); exit 1; }

test:
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor test -- cargo test --workspace

test-crate:
	@test -n "$(p)" || { echo 'usage: make test-crate p=<crate>'; exit 2; }
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor test-$(p) -- cargo test -p $(p)

mutants-gate:
	cargo run --release -p bt-mutants-gate -- $(OUTCOMES) $(ALLOWLIST)

mutants-gate-selftest:
	cargo test -p bt-mutants-gate

fuzz:
	$(MAKE_CMD) jobwrap
	cd fuzz && for t in $(FUZZ_TARGETS); do $(CURDIR)/$(JOBWRAP) --anchor fuzz-$$t -- cargo fuzz run $$t || exit 1; done

mutation:
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor mutation -- cargo mutants --jobs 1 $(addprefix -p ,$(MUTATION_PACKAGES))

mutation-pkg:
	@test -n "$(p)" || { echo 'usage: make mutation-pkg p=<crate>'; exit 2; }
	$(MAKE_CMD) jobwrap
	mkdir -p $(MUTATION_PKG_DIR)/$(p)
	$(JOBWRAP) --anchor mutation-pkg-$(p) -- cargo mutants --jobs 1 -p $(p) -o $(MUTATION_PKG_DIR)/$(p)

mutation-merge:
	cargo run --release -p bt-mutants-gate -- merge $(OUTCOMES) $(MUTATION_PKG_OUTS)

e2e: build
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor e2e -- cargo test --release -p bt-app --test e2e
	$(MAKE_CMD) e2e-smoke

e2e-smoke: build
	cargo build --release --manifest-path tests/e2e/Cargo.toml
	mkdir -p $(ARTIFACTS)/e2e $(ARTIFACTS)/e2e-user
	APPDATA=$(ARTIFACTS)/e2e-user $(E2E_BIN) $(BIN) $(ARTIFACTS)/e2e

visual: visual-selftest
	@if $(MAKE) visual-diff; then exit 0; fi; \
	 echo 'visual: mismatch, re-rendering and re-diffing once'; \
	 if $(MAKE) e2e-smoke && $(MAKE) visual-diff; then \
	   printf '\nverdict: infra flake, second run passed\n' >> $(ARTIFACTS)/visual/report.md; exit 3; \
	 else \
	   printf '\nverdict: regression, both runs mismatched\n' >> $(ARTIFACTS)/visual/report.md; exit 2; \
	 fi

visual-diff:
	cargo run --release -p bt-visual-diff -- $(ARTIFACTS)/e2e tests/visual/baselines

visual-selftest:
	cargo run --release -p bt-visual-diff -- --selftest $(ARTIFACTS)/visual-selftest

baselines:
	mkdir -p tests/visual/baselines
	cp $(ARTIFACTS)/e2e/*.png tests/visual/baselines/

screenshots: build
	rm -rf $(ARTIFACTS)/usage-user
	rm -f docs/screenshots/*.png
	mkdir -p docs/screenshots
	APPDATA=$(ARTIFACTS)/usage-user $(BIN) --usage docs/screenshots
	BT_CONFIG=$(ARTIFACTS)/usage-absent-config.json $(BIN) --fatal-shot docs/screenshots

memguard:
	pwsh -NoProfile -ExecutionPolicy Bypass -File $(MEMGUARD)

memguard-selftest:
	cargo build --release -p bt-sleeper
	pwsh -NoProfile -ExecutionPolicy Bypass -File $(MEMGUARD) -SelfTest -SleeperPath target/release/bt-sleeper.exe

clean-rust:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-apply:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -Apply -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-deep-apply:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -Apply -Deep -CoverageTarget $(COVERAGE_TARGET) -MutationTarget $(MUTATION_TARGET)

clean-rust-selftest:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/clean-rust.ps1 -SelfTest -WorkDir $(ARTIFACTS)/clean-rust-selftest

run:
	cargo run --release -p bt-app

chain:
	$(MAKE_CMD) jobwrap
	$(JOBWRAP) --anchor chain -- $(MAKE_CMD) test fmt lint policy check build fuzz mutation mutants-gate e2e baselines visual verify-algs

coverage:
	CARGO_TARGET_DIR=$(COVERAGE_TARGET) pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/coverage-badge.ps1

alg-coverage:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/alg-coverage-badge.ps1

diagrams:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/diagrams.ps1

release-verify:
	@test -n "$(v)" || { echo 'usage: make release-verify v=<tag>'; exit 2; }
	cargo run --release -p bt-release-verify -- $(ARTIFACTS)/dist/blind-trainer $(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip --run

release-verify-selftest:
	cargo test -p bt-release-verify

dist: build
	@test -n "$(v)" || { echo 'usage: make dist v=<tag>'; exit 2; }
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/dist.ps1 -Version $(v)
	$(MAKE) release-verify v=$(v)

release:
	@test -n "$(v)" || { echo 'usage: make release v=<tag>'; exit 2; }
	@test -f docs/releases/$(v).md || { echo missing docs/releases/$(v).md; exit 2; }
	@test -f $(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip || { echo run make dist v=$(v) first; exit 2; }
	git push origin main
	git push origin $(v)
	gh release create $(v) $(ARTIFACTS)/dist/blind-trainer-$(v)-windows-x64.zip --title "blind trainer $(v)" --notes-file docs/releases/$(v).md

clean:
	rm -rf $(ARTIFACTS) tests/e2e/target
	mkdir -p $(ARTIFACTS)
	touch $(ARTIFACTS)/.gitkeep
