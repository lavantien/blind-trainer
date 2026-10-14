REF_GROUND := ref_ground
CONFIG := config/config.json
ARTIFACTS := artifacts
BIN := target/release/blind-trainer.exe
FUZZ_TARGETS := notation classify memo commutator
MUTATION_PACKAGES := bt-core bt-scheme bt-data bt-ga

export BT_REF_GROUND := $(REF_GROUND)
export BT_CONFIG := $(CONFIG)
export BT_ARTIFACTS := $(ARTIFACTS)

.PHONY: setup docs-fetch gen-data verify-algs synth-simcheck memofreq ga fmt lint check build test test-crate fuzz mutation e2e visual baselines screenshots run chain coverage alg-coverage diagrams

COVERAGE_TARGET := C:/Users/lavantien/dev/github/bt-coverage-target
MUTATION_TARGET := C:/Users/lavantien/dev/github/bt-mutation-target

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

synth-simcheck:
	python -I scripts/synth_simcheck.py

memofreq:
	cargo run --release -p bt-memofreq

ga:
	cargo run --release -p bt-ga-run

fmt:
	cargo fmt --all -- --check

lint:
	cargo clippy --workspace --all-targets -- -D warnings

check:
	cargo check --workspace

build:
	cargo build --release -p bt-app
	test -f $(BIN) || { echo missing $(BIN); exit 1; }

test:
	cargo test --workspace

test-crate:
	cargo test -p $(p)

fuzz:
	cd fuzz && for t in $(FUZZ_TARGETS); do cargo fuzz run $$t || exit 1; done

mutation:
	CARGO_TARGET_DIR=$(MUTATION_TARGET) cargo mutants --jobs 1 $(addprefix -p ,$(MUTATION_PACKAGES))

e2e:
	cargo test --release -p bt-app --test e2e
	mkdir -p $(ARTIFACTS)/e2e
	$(BIN) --smoke $(ARTIFACTS)/e2e

visual:
	cargo run --release -p bt-visual-diff -- $(ARTIFACTS)/e2e tests/visual/baselines

baselines:
	mkdir -p tests/visual/baselines
	cp $(ARTIFACTS)/e2e/*.png tests/visual/baselines/

screenshots: build
	rm -rf $(ARTIFACTS)/usage-user
	rm -f docs/screenshots/*.png
	mkdir -p docs/screenshots
	APPDATA=$(ARTIFACTS)/usage-user $(BIN) --usage docs/screenshots
	BT_CONFIG=$(ARTIFACTS)/usage-absent-config.json $(BIN) --fatal-shot docs/screenshots

run:
	cargo run --release -p bt-app

chain:
	$(MAKE) test
	$(MAKE) fmt
	$(MAKE) lint
	$(MAKE) check
	$(MAKE) build
	$(MAKE) fuzz
	$(MAKE) mutation
	$(MAKE) e2e
	$(MAKE) baselines
	$(MAKE) visual
	$(MAKE) verify-algs

coverage:
	CARGO_TARGET_DIR=$(COVERAGE_TARGET) pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/coverage-badge.ps1

alg-coverage:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/alg-coverage-badge.ps1

diagrams:
	pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/diagrams.ps1
