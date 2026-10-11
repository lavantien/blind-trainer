# fuzz

Cargo-fuzz project for the engine crates. Targets: notation (byte strings through `Alg::parse`, format and reparse equality, inverse law), classify (fuzzer bytes repaired into piece-cohesive cube permutations through `classify` and `trace`), memo (fuzzer-derived seeds through `random_state`, `memo_from_array`, `apply_memo` with identity round trip).

Layout: `make fuzz` runs `cargo fuzz run` from inside this directory, and cargo-fuzz resolves its project as nearest ancestor with a Cargo.toml plus `/fuzz`, so `Cargo.toml` here is a nested workspace holding the real project in `fuzz/`. `rust-toolchain.toml` pins nightly for the whole subprocess tree.

Bounds: targets read `fuzz.runs` and `fuzz.secs` from `BT_CONFIG` (default discovery walks ancestors for `config/config.json`) and self-terminate with exit code 0 once either limit is reached, so the unbounded make invocation stays bounded.

Host requirements, this machine has a standalone Rust install shadowing the rustup proxies in PATH:
- `~/.cargo/bin` must precede the standalone Rust bin dir so the rustup proxy cargo resolves `rust-toolchain.toml`.
- The MSVC ASan runtime DLL dir (VS C++ AddressSanitizer component, `clang_rt.asan_dynamic-x86_64.dll`) must be on PATH.

Fallback without those: `cargo run --bin standalone` inside `fuzz/fuzz` drives the same target functions with the same config run count and time cap, without sanitizers.
