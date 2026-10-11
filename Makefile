REF_GROUND ?= ref_ground
CONFIG ?= config/config.json
ARTIFACTS ?= artifacts
BIN := target/release/blind-trainer.exe
FUZZ_TARGETS := notation classify memo
MUTATION_PACKAGES := bt-core bt-scheme bt-data bt-ga

export BT_REF_GROUND := $(REF_GROUND)
export BT_CONFIG := $(CONFIG)
export BT_ARTIFACTS := $(ARTIFACTS)

.PHONY: setup docs-fetch gen-data verify-algs memofreq ga fmt lint check build test fuzz mutation e2e visual run chain

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

fuzz:
	cd fuzz && for t in $(FUZZ_TARGETS); do cargo fuzz run $$t || exit 1; done

mutation:
	cargo mutants $(addprefix -p ,$(MUTATION_PACKAGES))

e2e:
	cargo test --release -p bt-app --test e2e
	mkdir -p $(ARTIFACTS)/e2e
	$(BIN) --smoke $(ARTIFACTS)/e2e

visual:
	cargo run --release -p bt-visual-diff -- $(ARTIFACTS)/e2e tests/visual/baselines

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
	$(MAKE) visual
	$(MAKE) verify-algs
