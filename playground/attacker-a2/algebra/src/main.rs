use bt_core::{Alg, Cube};
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_data::verify::verify;
use bt_scheme::{
    CaseId, CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, enumerate_cases, name_of,
    sticker_of,
};
use std::collections::BTreeMap;
use std::path::Path;

fn buffer_of(orbit: Orbit) -> u8 {
    match orbit {
        Orbit::Edge => EDGE_BUFFER_STICKER,
        Orbit::Corner => CORNER_BUFFER_STICKER,
    }
}

fn sheet_of(orbit: Orbit) -> SheetClass {
    match orbit {
        Orbit::Edge => SheetClass::Edge,
        Orbit::Corner => SheetClass::Corner,
    }
}

fn position_of(case: &CaseId) -> String {
    format!(
        "{}-{}-{}",
        name_of(buffer_of(case.orbit)),
        name_of(case.t1),
        name_of(case.t2)
    )
}

fn flip(case: &CaseId) -> CaseId {
    CaseId {
        orbit: case.orbit,
        t1: case.t2,
        t2: case.t1,
    }
}

fn spec_even_reps(n: u32) -> u32 {
    std::cmp::max(2, n + (n % 2))
}

struct Side {
    position: String,
    alg: Alg,
    vendor: bool,
    solver: &'static str,
}

fn spec_sides(corpus: &Corpus, case: &CaseId) -> Option<(Side, Side)> {
    let sheet = sheet_of(case.orbit);
    let rows = corpus.rows_for_position(sheet, &position_of(case));
    let a_row = rows.first()?;
    let a = Side {
        position: position_of(case),
        alg: a_row.alg.clone(),
        vendor: a_row.solver != Solver::User,
        solver: a_row.solver.dir(),
    };
    let flipped = flip(case);
    let flip_rows = corpus.rows_for_position(sheet, &position_of(&flipped));
    let mut b = match flip_rows.first() {
        Some(row) => Side {
            position: position_of(&flipped),
            alg: row.alg.clone(),
            vendor: row.solver != Solver::User,
            solver: row.solver.dir(),
        },
        None => Side {
            position: position_of(&flipped),
            alg: a.alg.inverse(),
            vendor: false,
            solver: "inverse",
        },
    };
    let mut check = Cube::solved();
    check.apply_alg(&a.alg);
    check.apply_alg(&b.alg);
    if !check.is_solved() {
        b = Side {
            position: position_of(&flipped),
            alg: a.alg.inverse(),
            vendor: false,
            solver: "inverse",
        };
    }
    Some((a, b))
}

fn alternates_from_prepared(a: &Alg, b: &Alg, reps: u32) -> (Cube, bool) {
    let mut cube = Cube::solved();
    cube.apply_alg(b);
    let prepared = cube;
    for index in 0..reps {
        cube.apply_alg(if index % 2 == 0 { a } else { b });
    }
    (cube, cube == prepared)
}

fn row(sheet: SheetClass, solver: Solver, position: &str, alg: &str) -> Row {
    Row {
        solver,
        sheet,
        rowid: 1,
        position: position.to_string(),
        alg: Alg::parse(alg).expect("fixture alg parses"),
        alg_text: alg.to_string(),
        commutator: String::new(),
        recon_link: String::new(),
        youtube_link: String::new(),
    }
}

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: sticker_of(Orbit::Edge, first),
        t2: sticker_of(Orbit::Edge, second),
    }
}

fn fail(label: &str, detail: String) {
    println!("FAIL {label}: {detail}");
}

fn main() {
    let root = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "../../..".to_string());
    let corpus = Corpus::load(&Path::new(&root).join("ref_ground"))
        .expect("corpus loads from ref_ground");

    let mut covered_cases = 0usize;
    let mut flip_covered = 0usize;
    let mut b_vendor_kept = 0usize;
    let mut b_guard_replaced = 0usize;
    let mut b_inverse_fallback = 0usize;
    let mut first_row_unverified: Vec<String> = Vec::new();
    let mut multi_row_positions = 0usize;
    let mut flip_multi_row_positions = 0usize;
    let mut alternation_failures = 0usize;
    let mut prepared_failures = 0usize;

    for orbit in [Orbit::Edge, Orbit::Corner] {
        let sheet = sheet_of(orbit);
        let mut positions: BTreeMap<String, usize> = BTreeMap::new();
        for row in corpus.rows().iter().filter(|row| row.sheet == sheet) {
            *positions.entry(row.position.clone()).or_insert(0) += 1;
        }
        for (position, count) in &positions {
            if *count > 1 {
                multi_row_positions += 1;
            }
        }
        let mut reachable: std::collections::HashSet<String> =
            corpus
                .rows()
                .iter()
                .filter(|row| row.sheet == sheet)
                .fold(std::collections::HashSet::new(), |mut set, row| {
                    set.insert(row.position.clone());
                    set
                });
        for case in enumerate_cases(orbit) {
            reachable.remove(&position_of(&case));
        }
        if !reachable.is_empty() {
            println!(
                "unreachable {sheet} rows by any case position: {:?}",
                reachable.iter().take(10).collect::<Vec<_>>()
            );
            println!("unreachable {sheet} count: {}", reachable.len());
        }

        for case in enumerate_cases(orbit) {
            let rows = corpus.rows_for_position(sheet, &position_of(&case));
            if rows.is_empty() {
                continue;
            }
            covered_cases += 1;
            if !verify(&rows[0].alg, &rows[0].position, rows[0].sheet).ok()
                && first_row_unverified.len() < 12
            {
                first_row_unverified.push(format!(
                    "{sheet} {} solver {} alg {}",
                    rows[0].position, rows[0].solver, rows[0].alg_text
                ));
            }
            let Some((a, b)) = spec_sides(&corpus, &case) else {
                fail("sides", format!("case {} lost sides", position_of(&case)));
                continue;
            };
            if b.solver != "inverse" {
                flip_covered += 1;
                if corpus
                    .rows_for_position(sheet, &position_of(&flip(&case)))
                    .len()
                    > 1
                {
                    flip_multi_row_positions += 1;
                }
                let raw_flip = corpus
                    .rows_for_position(sheet, &position_of(&flip(&case)))[0]
                    .alg
                    .clone();
                let mut cube = Cube::solved();
                cube.apply_alg(&a.alg);
                cube.apply_alg(&raw_flip);
                if cube.is_solved() {
                    b_vendor_kept += 1;
                } else {
                    b_guard_replaced += 1;
                }
            } else {
                b_inverse_fallback += 1;
            }
            let mut prepared = Cube::solved();
            prepared.apply_alg(&b.alg);
            let mut after_a = prepared;
            after_a.apply_alg(&a.alg);
            if !after_a.is_solved() {
                prepared_failures += 1;
                fail("prepared", format!("{} A on prepared not solved", a.position));
            }
            let mut after_two = after_a;
            after_two.apply_alg(&b.alg);
            if after_two != prepared {
                alternation_failures += 1;
                fail(
                    "alternation",
                    format!("{} reps 2 did not return to prepared", a.position),
                );
            }
            for reps in [3u32, 4, 5, 6, 7, 8, 100, 101] {
                let (cube, _) = alternates_from_prepared(&a.alg, &b.alg, spec_even_reps(reps));
                let mut expected = Cube::solved();
                expected.apply_alg(&b.alg);
                if cube != expected {
                    fail(
                        "even_reps",
                        format!("{} reps {} broke prepared return", a.position, reps),
                    );
                }
            }
        }
    }

    println!("covered cases: {covered_cases}");
    println!("flip covered: {flip_covered} kept {b_vendor_kept} guard replaced {b_guard_replaced}");
    println!("inverse fallback (no flip coverage): {b_inverse_fallback}");
    println!("multi row positions (A side alts): {multi_row_positions}");
    println!("flip positions with multiple rows: {flip_multi_row_positions}");
    println!("alternation failures: {alternation_failures}");
    println!("prepared failures: {prepared_failures}");
    println!("first rows failing verify: {}", first_row_unverified.len());
    for line in &first_row_unverified {
        println!("  {line}");
    }

    let ab = edge_case('A', 'B');
    let mut flip_rows_first_broken = Corpus::from_rows(vec![
        row(SheetClass::Edge, Solver::Cherry, "UF-UB-UR", "R2 U' S R2 S' R2 U R2"),
        row(SheetClass::Edge, Solver::Cherry, "UF-UR-UB", "R U R' U'"),
        row(
            SheetClass::Edge,
            Solver::Eggins,
            "UF-UR-UB",
            "R2 U' R2 S R2 S' U R2",
        ),
    ]);
    let (a, b) = spec_sides(&flip_rows_first_broken, &ab).expect("sides");
    println!(
        "adversarial multi-flip-row first broken: b solver {} alg {}",
        b.solver,
        b.alg.format()
    );
    let mut cube = Cube::solved();
    cube.apply_alg(&a.alg);
    cube.apply_alg(&b.alg);
    println!("adversarial multi-flip-row a;b solved: {}", cube.is_solved());

    let mut a_multi = Corpus::from_rows(vec![
        row(SheetClass::Edge, Solver::Eggins, "UF-UB-UR", "M2 U M2 U2 M U M2"),
        row(SheetClass::Edge, Solver::Cherry, "UF-UB-UR", "R2 U' S R2 S' R2 U R2"),
    ]);
    let (a, b) = spec_sides(&a_multi, &ab).expect("sides");
    println!(
        "A multi row picks first inserted: solver {} alg {}",
        a.solver,
        a.alg.format()
    );

    let mut user_only = Corpus::from_rows(vec![row(
        SheetClass::Edge,
        Solver::User,
        "UF-UB-UR",
        "R2 U' S R2 S' R2 U R2",
    )]);
    let (a, b) = spec_sides(&user_only, &ab).expect("sides");
    println!(
        "user only A: vendor {} solver {}",
        a.vendor, a.solver
    );

    let mut identity_a = Corpus::from_rows(vec![row(
        SheetClass::Edge,
        Solver::Cherry,
        "UF-UB-UR",
        "",
    )]);
    let (a, b) = spec_sides(&identity_a, &ab).expect("sides");
    let mut prepared = Cube::solved();
    prepared.apply_alg(&b.alg);
    println!(
        "identity A: prepared solved {} alg empty {}",
        prepared.is_solved(),
        b.alg.is_empty()
    );

    let mut user_flip_only = Corpus::from_rows(vec![
        row(SheetClass::Edge, Solver::Cherry, "UF-UB-UR", "R2 U' S R2 S' R2 U R2"),
        row(
            SheetClass::Edge,
            Solver::User,
            "UF-UR-UB",
            "R2 U' R2 S R2 S' U R2",
        ),
    ]);
    let (a, b) = spec_sides(&user_flip_only, &ab).expect("sides");
    let mut cube = Cube::solved();
    cube.apply_alg(&a.alg);
    cube.apply_alg(&b.alg);
    println!(
        "user-only flip coverage: b solver {} vendor {} a;b solved {}",
        b.solver,
        b.vendor,
        cube.is_solved()
    );

    let mut vendor_flip_after_user = Corpus::from_rows(vec![
        row(SheetClass::Edge, Solver::Cherry, "UF-UB-UR", "R2 U' S R2 S' R2 U R2"),
        row(
            SheetClass::Edge,
            Solver::User,
            "UF-UR-UB",
            "R U R' U'",
        ),
        row(
            SheetClass::Edge,
            Solver::Eggins,
            "UF-UR-UB",
            "R2 U' R2 S R2 S' U R2",
        ),
    ]);
    let (a, b) = spec_sides(&vendor_flip_after_user, &ab).expect("sides");
    let mut cube = Cube::solved();
    cube.apply_alg(&a.alg);
    cube.apply_alg(&b.alg);
    println!(
        "user row inserted before vendor flip: b solver {} a;b solved {}",
        b.solver,
        cube.is_solved()
    );

    a_multi.rows();
    flip_rows_first_broken.rows();
    user_only.rows();
    identity_a.rows();
    user_flip_only.rows();
    vendor_flip_after_user.rows();
}
