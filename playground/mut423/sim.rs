

const EDGE_STICKERS: [[u8; 2]; 12] = [
    [24, 40],
    [25, 36],
    [26, 32],
    [27, 28],
    [41, 31],
    [43, 37],
    [33, 39],
    [35, 29],
    [46, 42],
    [45, 38],
    [44, 34],
    [47, 30],
];

const CORNER_STICKERS: [[u8; 3]; 8] = [
    [1, 16, 13],
    [0, 17, 4],
    [2, 9, 12],
    [3, 8, 5],
    [22, 19, 14],
    [23, 18, 7],
    [21, 10, 15],
    [20, 11, 6],
];

const CORNER_SUCC: [[u8; 3]; 8] = [
    [1, 13, 16],
    [0, 17, 4],
    [2, 9, 12],
    [3, 5, 8],
    [22, 19, 14],
    [23, 7, 18],
    [21, 15, 10],
    [20, 11, 6],
];

const EDGE_FLOATING_PIECES: [u8; 12] = [2, 0, 1, 3, 6, 7, 10, 8, 9, 11, 5, 4];
const CORNER_FLOATING_PIECES: [u8; 8] = [2, 3, 0, 1, 6, 7, 4, 5];

const EDGE_BUFFER_STICKER: u8 = 26;
const CORNER_BUFFER_STICKER: u8 = 2;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Orbit {
    Edge,
    Corner,
}

fn piece_index(sticker: u8) -> u8 {
    if sticker < 24 {
        for (p, cyc) in CORNER_STICKERS.iter().enumerate() {
            if cyc.contains(&sticker) {
                return p as u8;
            }
        }
    } else {
        for (p, pair) in EDGE_STICKERS.iter().enumerate() {
            if pair.contains(&sticker) {
                return p as u8;
            }
        }
    }
    panic!("unknown sticker {sticker}");
}

fn same_piece(a: u8, b: u8) -> bool {
    if (a < 24) != (b < 24) {
        return false;
    }
    piece_index(a) == piece_index(b)
}

fn letter_of(sticker: u8) -> char {
    (b'A' + sticker % 24) as char
}

fn head_of(orbit: Orbit, piece: u8) -> u8 {
    match orbit {
        Orbit::Corner => CORNER_STICKERS[piece as usize][0],
        Orbit::Edge => EDGE_STICKERS[piece as usize][0],
    }
}

fn stickers_of(orbit: Orbit, piece: u8) -> Vec<u8> {
    match orbit {
        Orbit::Corner => CORNER_STICKERS[piece as usize].to_vec(),
        Orbit::Edge => EDGE_STICKERS[piece as usize].to_vec(),
    }
}

fn solved_piece(occ: &[u8; 54], orbit: Orbit, piece: u8) -> bool {
    stickers_of(orbit, piece)
        .iter()
        .all(|&p| occ[p as usize] == p)
}

fn in_place(occ: &[u8; 54], orbit: Orbit, piece: u8) -> bool {
    let homes = stickers_of(orbit, piece);
    let mut occupants: Vec<u8> = homes.iter().map(|&p| occ[p as usize]).collect();
    let mut expected = homes.clone();
    occupants.sort_unstable();
    expected.sort_unstable();
    occupants == expected
}

fn edge_mate(pos: u8) -> u8 {
    let pair = EDGE_STICKERS[piece_index(pos) as usize];
    if pair[0] == pos {
        pair[1]
    } else {
        pair[0]
    }
}

fn corner_succ(pos: u8) -> u8 {
    let cyc = &CORNER_SUCC[piece_index(pos) as usize];
    let i = cyc.iter().position(|&s| s == pos).unwrap();
    cyc[(i + 1) % 3]
}

fn pair_cycles(orbit: Orbit, h: u8, t1: u8, t2: u8) -> Vec<[u8; 3]> {
    match orbit {
        Orbit::Edge => vec![[h, t1, t2], [edge_mate(h), edge_mate(t1), edge_mate(t2)]],
        Orbit::Corner => {
            let (mut sh, mut s1, mut s2) = (h, t1, t2);
            let mut cycles = Vec::new();
            for _ in 0..3 {
                cycles.push([sh, s1, s2]);
                sh = corner_succ(sh);
                s1 = corner_succ(s1);
                s2 = corner_succ(s2);
            }
            cycles
        }
    }
}

fn apply_sticker_cycles(occ: &mut [u8; 54], cycles: &[[u8; 3]]) {
    for &[a, b, c] in cycles {
        let (sa, sb, sc) = (occ[a as usize], occ[b as usize], occ[c as usize]);
        occ[b as usize] = sa;
        occ[c as usize] = sb;
        occ[a as usize] = sc;
    }
}

fn assert_case_legal(orbit: Orbit, t1: u8, t2: u8) -> Result<(), &'static str> {
    let buffer = match orbit {
        Orbit::Corner => CORNER_BUFFER_STICKER,
        Orbit::Edge => EDGE_BUFFER_STICKER,
    };
    let in_orbit = |s: u8| match orbit {
        Orbit::Corner => s < 24,
        Orbit::Edge => (24..48).contains(&s),
    };
    if !in_orbit(t1) {
        return Err("t1 outside orbit");
    }
    if !in_orbit(t2) {
        return Err("t2 outside orbit");
    }
    if same_piece(t1, buffer) {
        return Err("t1 on buffer piece");
    }
    if same_piece(t2, buffer) {
        return Err("t2 on buffer piece");
    }
    if same_piece(t1, t2) {
        return Err("t2 on t1 piece");
    }
    Ok(())
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct WalkOutcome {
    letters: Vec<char>,
    pairs: Vec<(u8, u8)>,
    buffers: Vec<u8>,
    parity: Option<(u8, u8)>,
    err: Option<String>,
    steps: u32,
}

struct Sim {
    orbit: Orbit,
    occ: [u8; 54],
    pieces: Vec<u8>,
    reserved: Vec<bool>,
    visited: [bool; 48],
    letters: Vec<char>,
    pairs: Vec<(u8, u8)>,
    buffers: Vec<u8>,
    pending: Vec<u8>,
    parity: Option<(u8, u8)>,
    case_legal: bool,
    spins_star: bool,
    budget_star: bool,
    step_cap: u32,
}

impl Sim {
    fn new(orbit: Orbit, occ: [u8; 54], case_legal: bool, spins_star: bool, budget_star: bool) -> Sim {
        let pieces = match orbit {
            Orbit::Corner => CORNER_FLOATING_PIECES.to_vec(),
            Orbit::Edge => EDGE_FLOATING_PIECES.to_vec(),
        };
        let count = pieces.len();
        Sim {
            orbit,
            occ,
            pieces,
            reserved: vec![false; count],
            visited: [false; 48],
            letters: Vec::new(),
            pairs: Vec::new(),
            buffers: Vec::new(),
            pending: Vec::new(),
            parity: None,
            case_legal,
            spins_star,
            budget_star,
            step_cap: 0,
        }
    }

    fn main_scan(&mut self) -> Option<u8> {
        for &p in &self.pieces {
            if self.reserved[p as usize] || solved_piece(&self.occ, self.orbit, p) {
                continue;
            }
            if in_place(&self.occ, self.orbit, p) {
                self.reserved[p as usize] = true;
                continue;
            }
            return Some(p);
        }
        None
    }

    fn read_block(&mut self) -> Option<u8> {
        for &p in &self.pieces {
            if self.reserved[p as usize] || solved_piece(&self.occ, self.orbit, p) {
                continue;
            }
            if in_place(&self.occ, self.orbit, p) {
                self.reserved[p as usize] = true;
                continue;
            }
            if stickers_of(self.orbit, p)
                .iter()
                .any(|&s| self.visited[s as usize])
            {
                continue;
            }
            return Some(p);
        }
        None
    }

    fn run(mut self) -> WalkOutcome {
        let mut budget = 0u32;
        let mut spins = 0u32;
        let mut err: Option<String> = None;
        'outer: loop {
            self.step_cap += 1;
            if self.step_cap > 400000 {
                err = Some(format!("HANG after letters {}", self.letters.len()));
                break 'outer;
            }
            spins = if self.spins_star { spins * 1 } else { spins + 1 };
            if spins > 200 {
                err = Some("memo walk spin budget exceeded".to_string());
                break 'outer;
            }
            let Some(buffer) = self.main_scan() else {
                self.pending.clear();
                break 'outer;
            };
            let h = head_of(self.orbit, buffer);
            if self
                .pending
                .first()
                .is_some_and(|&t| self.reserved[piece_index(t) as usize])
            {
                self.pending.clear();
            }
            let mut pos = if self.pending.is_empty() {
                self.visited[h as usize] = true;
                h
            } else {
                match self.read_block() {
                    Some(q) => {
                        let qh = head_of(self.orbit, q);
                        self.visited[qh as usize] = true;
                        qh
                    }
                    None => {
                        let t = self.pending[0];
                        self.parity = Some((h, head_of(self.orbit, piece_index(t))));
                        break 'outer;
                    }
                }
            };
            loop {
                self.step_cap += 1;
                if self.step_cap > 400000 {
                    err = Some(format!("HANG after letters {}", self.letters.len()));
                    break 'outer;
                }
                let s = self.occ[pos as usize];
                if same_piece(s, h) {
                    break;
                }
                self.pending.push(s);
                self.letters.push(letter_of(s));
                self.visited[s as usize] = true;
                budget = if self.budget_star { budget * 1 } else { budget + 1 };
                if budget > 96 {
                    err = Some("memo walk letter budget exceeded".to_string());
                    break 'outer;
                }
                if self.pending.len() == 2 {
                    let (t1, t2) = (self.pending[0], self.pending[1]);
                    if self.case_legal {
                        if let Err(msg) = assert_case_legal(self.orbit, t1, t2) {
                            err = Some(format!(
                                "{msg}: CaseId {{ orbit: Edge, t1: {t1}, t2: {t2} }}"
                            ));
                            break 'outer;
                        }
                    }
                    self.pairs.push((t1, t2));
                    self.buffers.push(h);
                    let cycles = pair_cycles(self.orbit, h, t1, t2);
                    apply_sticker_cycles(&mut self.occ, &cycles);
                    self.pending.clear();
                    pos = h;
                } else {
                    pos = s;
                }
            }
        }
        WalkOutcome {
            letters: self.letters,
            pairs: self.pairs,
            buffers: self.buffers,
            parity: self.parity,
            err,
            steps: self.step_cap,
        }
    }
}

fn identity() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn occ_of(assignments: &[(u8, u8)]) -> [u8; 54] {
    let mut occ = identity();
    for &(pos, sticker) in assignments {
        occ[pos as usize] = sticker;
    }
    occ
}

fn show(tag: &str, o: &WalkOutcome) {
    println!(
        "{tag}: err={:?} letters={:?} pairs={:?} buffers={:?} parity={:?} steps={}",
        o.err, o.letters, o.pairs, o.buffers, o.parity, o.steps
    );
}

fn main() {
    let reserved_input = occ_of(&[
        (24, 47),
        (40, 30),
        (47, 24),
        (30, 40),
        (25, 27),
        (36, 28),
        (27, 25),
        (28, 36),
    ]);
    let mut sim = Sim::new(Orbit::Edge, reserved_input, true, false, false);
    sim.reserved[0] = true;
    show("reserved_input clean", &sim.run());

    let dup_input = occ_of(&[(26, 24), (24, 24)]);
    show("dup_input case_legal on", &Sim::new(Orbit::Edge, dup_input, true, false, false).run());
    show("dup_input case_legal off", &Sim::new(Orbit::Edge, dup_input, false, false, false).run());

    let spin_input = occ_of(&[(2, 9), (9, 9)]);
    let mut corner_sim = Sim::new(Orbit::Corner, spin_input, true, false, false);
    corner_sim.pieces = CORNER_FLOATING_PIECES.to_vec();
    let _ = corner_sim;
    show(
        "corner_spin_input clean",
        &Sim::new(Orbit::Corner, spin_input, true, false, false).run(),
    );
    show(
        "corner_spin_input spins*=1",
        &Sim::new(Orbit::Corner, spin_input, true, true, false).run(),
    );

    search_edge_loop();
    find_budget_case();
}

fn search_edge_loop() {
    let active: Vec<u8> = vec![26, 32, 24, 40, 25, 36, 27, 28];
    let n = active.len();
    let mut best = 0usize;
    let mut best_occ = None;
    let mut assignment = vec![0u8; n];
    let total = (n as u64).pow(n as u32);
    for idx in 0..total {
        let mut x = idx;
        for slot in 0..n {
            assignment[slot] = active[(x % n as u64) as usize];
            x /= n as u64;
        }
        let mut occ = identity();
        for (slot, &sticker) in assignment.iter().enumerate() {
            occ[active[slot] as usize] = sticker;
        }
        let sim = Sim::new(Orbit::Edge, occ, true, false, false);
        let outcome = sim.run();
        if outcome.err.is_none() && outcome.letters.len() > best {
            best = outcome.letters.len();
            best_occ = Some((occ, outcome.clone()));
        }
        if outcome.err.is_none() && outcome.letters.len() > 96 {
            println!("FOUND letters={:?}", outcome.letters);
            println!("occ assignments: {:?}", assignment);
            show("found", &outcome);
            return;
        }
    }
    println!("no loop in {}^{} functions; best clean letters = {}", n, n, best);
    if let Some((occ, _)) = best_occ {
        let diffs: Vec<(u8, u8)> = (0..54u8)
            .filter(|&i| occ[i as usize] != i)
            .map(|i| (i, occ[i as usize]))
            .collect();
        println!("best occ diffs: {:?}", diffs);
    }
}

fn find_budget_case() {
    let support: Vec<u8> = vec![26, 32, 24, 40, 25, 36, 27, 28, 33, 39];
    let mut rng = XorShift(0x9E3779B97F4A7C15);
    for _ in 0..20_000_000u64 {
        let mut occ = identity();
        for &slot in &support {
            occ[slot as usize] = support[rng.below(support.len())];
        }
        let outcome = Sim::new(Orbit::Edge, occ, true, false, false).run();
        if outcome.err.as_deref() == Some("memo walk letter budget exceeded") {
            let diffs: Vec<(u8, u8)> = (24..48u8)
                .filter(|&i| occ[i as usize] != i)
                .map(|i| (i, occ[i as usize]))
                .collect();
            println!("budget case occ diffs: {diffs:?}");
            show("budget case clean", &outcome);
            let mutant = Sim::new(Orbit::Edge, occ, true, false, true).run();
            show("budget case budget*=1", &mutant);
            return;
        }
    }
    println!("no budget case found");
}

struct XorShift(u64);

impl XorShift {
    fn next(&mut self) -> u64 {
        let mut z = self.0;
        z ^= z << 13;
        z ^= z >> 7;
        z ^= z << 17;
        self.0 = z;
        z
    }
    fn below(&mut self, n: usize) -> usize {
        ((self.next() >> 32) as u128 * n as u128 >> 32) as usize
    }
}

fn random_search(tag: &str, orbit: Orbit, support: &[u8], samples: u64, perm_mode: bool) {
    let mut rng = XorShift(0x9E3779B97F4A7C15);
    let mut best = 0usize;
    let mut best_occ = None;
    let mut budget_hits = 0u64;
    let mut spin_hits = 0u64;
    let mut hang_hits = 0u64;
    for _ in 0..samples {
        let mut occ = identity();
        if perm_mode {
            let mut vals = support.to_vec();
            for i in (1..vals.len()).rev() {
                let j = rng.below(i + 1);
                vals.swap(i, j);
            }
            for (slot, &sticker) in support.iter().zip(vals.iter()) {
                occ[(*slot as usize)] = sticker;
            }
        } else {
            for &slot in support {
                occ[slot as usize] = support[rng.below(support.len())];
            }
        }
        let sim = Sim::new(orbit, occ, true, false, false);
        let outcome = sim.run();
        let letters = outcome.letters.len();
        if letters > 96 {
            budget_hits += 1;
        }
        match outcome.err.as_deref() {
            Some(e) if e.contains("spin budget") => spin_hits += 1,
            Some(e) if e.starts_with("HANG") => hang_hits += 1,
            _ => {}
        }
        if outcome.err.is_none() && letters > best {
            best = letters;
            best_occ = Some(occ);
        }
    }
    println!(
        "{tag}: samples={samples} perm={perm_mode} best_letters={best} budget_panics={budget_hits} spin_panics={spin_hits} hangs={hang_hits}"
    );
    if let Some(occ) = best_occ {
        let diffs: Vec<(u8, u8)> = (0..54u8)
            .filter(|&i| occ[i as usize] != i)
            .map(|i| (i, occ[i as usize]))
            .collect();
        println!("{tag} best occ diffs: {diffs:?}");
    }
}

fn search_random() {
    let edge_all: Vec<u8> = (24..48u8).collect();
    let edge10: Vec<u8> = vec![26, 32, 24, 40, 25, 36, 27, 28, 33, 39];
    random_search("edge-10-fn", Orbit::Edge, &edge10, 20_000_000, false);
    random_search("edge-10-perm", Orbit::Edge, &edge10, 20_000_000, true);
    random_search("edge-24-fn", Orbit::Edge, &edge_all, 20_000_000, false);
    random_search("edge-24-perm", Orbit::Edge, &edge_all, 20_000_000, true);
    let corner_all: Vec<u8> = (0..24u8).collect();
    let corner9: Vec<u8> = vec![2, 9, 12, 1, 16, 13, 0, 17, 4];
    random_search("corner-9-fn", Orbit::Corner, &corner9, 20_000_000, false);
    random_search("corner-9-perm", Orbit::Corner, &corner9, 20_000_000, true);
    random_search("corner-24-fn", Orbit::Corner, &corner_all, 20_000_000, false);
    random_search("corner-24-perm", Orbit::Corner, &corner_all, 20_000_000, true);
}
