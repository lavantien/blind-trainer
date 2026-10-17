use std::time::{Duration, Instant};

use bt_fuzz::{classify_once, expand_once, fuzz_limits, memo_once, notation_once};
use bt_scheme::Rng;

fn bytes_for(rng: &mut Rng) -> Vec<u8> {
    let len = rng.below(97) as usize;
    let mut bytes = Vec::with_capacity(len);
    while bytes.len() < len {
        bytes.extend_from_slice(&rng.next_u64().to_le_bytes());
    }
    bytes.truncate(len);
    bytes
}

const ALG_ALPHABET: &[u8] = b"RULFDBMESxyz2'()[]+:, urrmm";

fn algish_bytes(rng: &mut Rng) -> Vec<u8> {
    let len = rng.below(97) as usize;
    (0..len)
        .map(|_| ALG_ALPHABET[rng.below(ALG_ALPHABET.len() as u64) as usize])
        .collect()
}

fn main() {
    let (runs, secs) = fuzz_limits();
    let Some(runs) = runs else {
        eprintln!("fuzz config missing or zero runs: set fuzz.runs in BT_CONFIG");
        std::process::exit(2);
    };
    let deadline = secs.map(|secs| Instant::now() + Duration::from_secs(secs));
    let mut executed = 0u64;
    for i in 0..runs {
        if let Some(deadline) = deadline
            && Instant::now() >= deadline
        {
            break;
        }
        let mut rng = Rng::new(i);
        let bytes = bytes_for(&mut rng);
        notation_once(&bytes);
        classify_once(&bytes);
        memo_once(&bytes);
        expand_once(&bytes);
        expand_once(&algish_bytes(&mut rng));
        executed += 1;
    }
    println!("standalone fuzz iterations: {executed}");
}
