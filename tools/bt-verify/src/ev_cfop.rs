use crate::ev_bld::Layout;
use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_core::{Alg, Cube, Face};
use bt_data::seedpack::SeedRow;

const U_INDEX: usize = 0;

fn face_of_code(code: u8) -> usize {
    if code < 24 {
        code as usize / 4
    } else if code < 48 {
        (code as usize - 24) / 4
    } else {
        code as usize - 48
    }
}

fn u_band(layout: &Layout) -> Vec<u8> {
    let u_bit = 1 << U_INDEX;
    (0..layout.piece_count())
        .filter(|index| layout.mask_of(*index) & u_bit != 0)
        .flat_map(|index| layout.slot_of_index(index).to_vec())
        .collect()
}

fn round_trip(alg: &Alg) -> Result<(), String> {
    let mut forward = Cube::solved();
    forward.apply_alg(alg);
    forward.apply_alg(&alg.inverse());
    if !forward.is_solved() {
        return Err("alg then inverse misses solved".to_string());
    }
    let mut reverse = Cube::solved();
    reverse.apply_alg(&alg.inverse());
    reverse.apply_alg(alg);
    if !reverse.is_solved() {
        return Err("inverse then alg misses solved".to_string());
    }
    Ok(())
}

fn case_cube(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&alg.inverse());
    cube
}

fn f2l_exact(state: &Cube, band: &[u8]) -> bool {
    let band_set: std::collections::HashSet<u8> = band.iter().copied().collect();
    (0..54u8).all(|position| band_set.contains(&position) || state.sticker_at(position) == position)
}

fn u_uniform(state: &Cube) -> bool {
    [0u8, 1, 2, 3, 24, 25, 26, 27, 48]
        .iter()
        .all(|position| face_of_code(state.sticker_at(*position)) == U_INDEX)
}

const ROTATION_TIPS: [&str; 6] = ["", "x", "x2", "x'", "z", "z'"];
const ROTATION_TWISTS: [&str; 4] = ["", "y", "y2", "y'"];

fn rotations() -> Vec<Alg> {
    let mut words = Vec::new();
    for tip in ROTATION_TIPS {
        for twist in ROTATION_TWISTS {
            let text = format!("{tip} {twist}").trim().to_string();
            words.push(Alg::parse(&text).expect("rotation word parses"));
        }
    }
    words
}

fn rotated_states(state: &Cube) -> Vec<Cube> {
    rotations()
        .iter()
        .map(|rotation| {
            let mut turned = *state;
            turned.apply_alg(rotation);
            turned
        })
        .collect()
}

fn cross_solved_on_some_face(state: &Cube, layout: &Layout) -> bool {
    (0..6).any(|face| {
        let bit = 1u8 << face;
        (0..layout.piece_count())
            .filter(|index| {
                let mask = layout.mask_of(*index);
                mask.count_ones() == 2 && mask & bit != 0
            })
            .all(|index| {
                layout
                    .slot_of_index(index)
                    .iter()
                    .all(|position| state.sticker_at(*position) == *position)
            })
    })
}

fn slot_pair_displaced(front_json: &str, state: &Cube, layout: &Layout) -> Result<bool, String> {
    let value: serde_json::Value =
        serde_json::from_str(front_json).map_err(|err| err.to_string())?;
    let slot = value
        .get("slot")
        .and_then(|slot| slot.as_str())
        .ok_or_else(|| "f2l row names no slot".to_string())?;
    let faces: Vec<usize> = slot
        .chars()
        .map(|ch| match ch {
            'u' | 'U' => Ok(Face::U.index()),
            'l' | 'L' => Ok(Face::L.index()),
            'f' | 'F' => Ok(Face::F.index()),
            'r' | 'R' => Ok(Face::R.index()),
            'b' | 'B' => Ok(Face::B.index()),
            'd' | 'D' => Ok(Face::D.index()),
            _ => Err(format!("slot {slot} holds a non-face letter")),
        })
        .collect::<Result<Vec<usize>, String>>()?;
    let mut slot_disturbed = false;
    for index in 0..layout.piece_count() {
        let mask = layout.mask_of(index);
        if mask & (1 << U_INDEX) != 0 {
            continue;
        }
        let home = layout
            .slot_of_index(index)
            .iter()
            .all(|position| state.sticker_at(*position) == *position);
        if home {
            continue;
        }
        let belongs = faces.iter().all(|face| mask & (1 << face) != 0);
        if belongs {
            slot_disturbed = true;
        }
    }
    Ok(slot_disturbed)
}

fn perm_of(text: &str) -> Result<[u8; 54], String> {
    let alg = Alg::parse(text).map_err(|err| err.to_string())?;
    let mut cube = Cube::solved();
    cube.apply_alg(&alg);
    let mut perm = [0u8; 54];
    for position in 0..54u8 {
        perm[position as usize] = cube.sticker_at(position);
    }
    Ok(perm)
}

const SUFFIXES: [&str; 4] = ["", "2", "'", "2'"];

fn split_suffix(token: &str) -> Result<(String, String), String> {
    let letter: String = token
        .chars()
        .take_while(|ch| ch.is_ascii_alphabetic() || *ch == 'w')
        .collect();
    let suffix = &token[letter.len()..];
    if letter.is_empty() || !SUFFIXES.contains(&suffix) {
        return Err(format!("token {token} holds no canonical move"));
    }
    Ok((letter, suffix.to_string()))
}

const FACE_LETTERS: [&str; 6] = ["U", "L", "F", "R", "B", "D"];
const WIDE_LETTERS: [&str; 6] = ["u", "l", "f", "r", "b", "d"];
const SLICE_LETTERS: [&str; 3] = ["M", "E", "S"];
const ROTATION_LETTERS: [&str; 3] = ["x", "y", "z"];

fn candidates_of(letter: &str) -> Option<&'static [&'static str]> {
    if FACE_LETTERS.contains(&letter) {
        Some(&FACE_LETTERS)
    } else if WIDE_LETTERS.contains(&letter)
        || (letter.len() == 2 && letter.ends_with('w') && FACE_LETTERS.contains(&&letter[..1]))
    {
        Some(&WIDE_LETTERS)
    } else if SLICE_LETTERS.contains(&letter) {
        Some(&SLICE_LETTERS)
    } else {
        None
    }
}

fn rewrite_to_fixed(alg_text: &str) -> Result<String, String> {
    let mut frame: Vec<String> = Vec::new();
    let mut out: Vec<String> = Vec::new();
    for token in alg_text.split_whitespace() {
        let (letter, _suffix) = split_suffix(token)?;
        if ROTATION_LETTERS.contains(&letter.as_str()) {
            frame.push(token.to_string());
            continue;
        }
        let frame_word = frame.join(" ");
        let target = if frame_word.is_empty() {
            perm_of(token)?
        } else {
            perm_of(&format!("{frame_word} {token}"))?
        };
        let Some(candidates) = candidates_of(&letter) else {
            return Err(format!("token {token} joins no move class"));
        };
        let mut matched = None;
        'outer: for candidate in candidates {
            for suffix in SUFFIXES {
                let fixed = format!("{candidate}{suffix}");
                let probe = if frame_word.is_empty() {
                    fixed.clone()
                } else {
                    format!("{fixed} {frame_word}")
                };
                if perm_of(&probe)? == target {
                    matched = Some(fixed);
                    break 'outer;
                }
            }
        }
        let Some(fixed) = matched else {
            return Err(format!("token {token} conjugates to no in-class move"));
        };
        out.push(fixed);
    }
    Ok(out.join(" "))
}

#[derive(Clone, Copy)]
enum CfopStage {
    Pll,
    Oll,
    F2l,
}

fn stage_of(stage: &str) -> Result<CfopStage, String> {
    match stage {
        "pll" => Ok(CfopStage::Pll),
        "oll" => Ok(CfopStage::Oll),
        "f2l" => Ok(CfopStage::F2l),
        other => Err(format!("stage {other} names no cfop step")),
    }
}

fn stage_outcome(
    stage: CfopStage,
    state: &Cube,
    layout: &Layout,
    front_json: &str,
) -> Tier2Outcome {
    if state.is_solved() {
        return Tier2Outcome::Fail("start state is already solved".to_string());
    }
    let frames = rotated_states(state);
    match stage {
        CfopStage::Pll => {
            let clean = frames
                .iter()
                .any(|frame| f2l_exact(frame, &u_band_positions(layout)) && u_uniform(frame));
            if !clean {
                return Tier2Outcome::Fail(
                    "no rotation frames a solved first two layers with an oriented last layer"
                        .to_string(),
                );
            }
            Tier2Outcome::Pass
        }
        CfopStage::Oll => {
            let clean = frames
                .iter()
                .any(|frame| f2l_exact(frame, &u_band_positions(layout)) && !u_uniform(frame));
            if !clean {
                return Tier2Outcome::Fail(
                    "no rotation frames a solved first two layers with a disoriented last layer"
                        .to_string(),
                );
            }
            Tier2Outcome::Pass
        }
        CfopStage::F2l => {
            if !cross_solved_on_some_face(state, layout) {
                return Tier2Outcome::Fail("the state holds no solved cross".to_string());
            }
            if f2l_exact(state, &u_band_positions(layout)) {
                return Tier2Outcome::Fail("the first two layers are already solved".to_string());
            }
            match slot_pair_displaced(front_json, state, layout) {
                Ok(true) => Tier2Outcome::Pass,
                Ok(false) => Tier2Outcome::Fail("the named slot holds its pair solved".to_string()),
                Err(err) => Tier2Outcome::Fail(err),
            }
        }
    }
}

fn u_band_positions(layout: &Layout) -> Vec<u8> {
    u_band(layout)
}

fn verify_cfop(rows: &[SeedRow], stage: &str) -> SectionResult {
    let layout = Layout::probe();
    let stage = match stage_of(stage) {
        Ok(stage) => stage,
        Err(err) => {
            return SectionResult {
                rows: rows
                    .iter()
                    .map(|row| RowOutcome {
                        case_key: row.case_key.clone(),
                        tier1: Err(err.clone()),
                        tier2: Tier2Outcome::Skipped("tier 1 failed"),
                    })
                    .collect(),
                honesty: Vec::new(),
            };
        }
    };
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let alg_text = row.alg.replace(['(', ')'], "");
        let tier1 = if alg_text.trim().is_empty() {
            Err("alg is empty".to_string())
        } else {
            Alg::parse(&alg_text)
                .map_err(|err| err.to_string())
                .and_then(|alg| round_trip(&alg))
        };
        let tier2 = match &tier1 {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok(()) => {
                let alg = Alg::parse(&alg_text).expect("tier 1 parsed the alg");
                let state = match rewrite_to_fixed(&alg_text) {
                    Ok(fixed) if !fixed.trim().is_empty() => {
                        let fixed_alg = Alg::parse(&fixed).expect("rewritten alg parses");
                        case_cube(&fixed_alg)
                    }
                    Ok(_) => case_cube(&alg),
                    Err(err) => {
                        outcomes.push(RowOutcome {
                            case_key: row.case_key.clone(),
                            tier1: Ok(()),
                            tier2: Tier2Outcome::Fail(err),
                        });
                        continue;
                    }
                };
                stage_outcome(stage, &state, &layout, &row.front_json)
            }
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1,
            tier2,
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: Vec::new(),
    }
}

pub fn verify_pll(rows: &[SeedRow]) -> SectionResult {
    verify_cfop(rows, "pll")
}

pub fn verify_oll(rows: &[SeedRow]) -> SectionResult {
    verify_cfop(rows, "oll")
}

pub fn verify_f2l(rows: &[SeedRow]) -> SectionResult {
    verify_cfop(rows, "f2l")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_u_band_holds_four_corners_and_four_edges() {
        let layout = Layout::probe();
        let band = u_band(&layout);
        assert_eq!(band.len(), 20);
        let mut after_u = Cube::solved();
        after_u.apply_alg(&Alg::parse("U").expect("u turn parses"));
        assert!(f2l_exact(&after_u, &band));
        let mut after_r = Cube::solved();
        after_r.apply_alg(&Alg::parse("R").expect("r turn parses"));
        assert!(!f2l_exact(&after_r, &band));
    }

    #[test]
    fn regrasp_moves_rewrite_to_their_fixed_frame_moves() {
        assert_eq!(rewrite_to_fixed("y L'").expect("y L' rewrites"), "F'");
        assert_eq!(rewrite_to_fixed("x U'").expect("x U' rewrites"), "F'");
        assert_eq!(
            rewrite_to_fixed("R U R'").expect("plain alg rewrites"),
            "R U R'"
        );
        assert!(rewrite_to_fixed("Q'").is_err());
    }

    #[test]
    fn a_t_perm_case_state_is_a_clean_pll() {
        let layout = Layout::probe();
        let alg = Alg::parse("R U R' U' R' F R2 U' R' U' R U R' F'").expect("t perm parses");
        let state = case_cube(&alg);
        assert!(matches!(
            stage_outcome(CfopStage::Pll, &state, &layout, "{\"case\":\"T\"}"),
            Tier2Outcome::Pass
        ));
        assert!(matches!(
            stage_outcome(CfopStage::Oll, &state, &layout, "{\"case\":\"T\"}"),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn a_sune_case_state_is_a_clean_oll() {
        let layout = Layout::probe();
        let alg = Alg::parse("R U R' U R U2 R'").expect("sune parses");
        let state = case_cube(&alg);
        assert!(matches!(
            stage_outcome(CfopStage::Oll, &state, &layout, "{\"case\":\"Sune\"}"),
            Tier2Outcome::Pass
        ));
        assert!(matches!(
            stage_outcome(CfopStage::Pll, &state, &layout, "{\"case\":\"Sune\"}"),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn a_simple_insert_case_keeps_the_cross_and_displaces_the_slot() {
        let layout = Layout::probe();
        let alg = Alg::parse("U R U' R'").expect("insert parses");
        let state = case_cube(&alg);
        assert!(matches!(
            stage_outcome(
                CfopStage::F2l,
                &state,
                &layout,
                "{\"case\":\"01\",\"slot\":\"fr\"}"
            ),
            Tier2Outcome::Pass
        ));
        let oll = Alg::parse("R U R' U R U2 R'").expect("sune parses");
        let oll_state = case_cube(&oll);
        assert!(matches!(
            stage_outcome(
                CfopStage::F2l,
                &oll_state,
                &layout,
                "{\"case\":\"01\",\"slot\":\"fr\"}"
            ),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn a_y_regrasp_insert_still_displaces_the_named_slot() {
        let layout = Layout::probe();
        let fixed = rewrite_to_fixed("y L' U' L").expect("regrasp insert rewrites");
        let alg = Alg::parse(&fixed).expect("rewritten insert parses");
        let state = case_cube(&alg);
        assert!(matches!(
            stage_outcome(
                CfopStage::F2l,
                &state,
                &layout,
                "{\"case\":\"10\",\"slot\":\"fr\"}"
            ),
            Tier2Outcome::Pass
        ));
    }

    #[test]
    fn solved_and_stage_names_resolve() {
        let layout = Layout::probe();
        let solved = Cube::solved();
        assert!(matches!(
            stage_outcome(CfopStage::Pll, &solved, &layout, "{}"),
            Tier2Outcome::Fail(_)
        ));
        assert!(matches!(stage_of("pll"), Ok(CfopStage::Pll)));
        assert!(matches!(stage_of("oll"), Ok(CfopStage::Oll)));
        assert!(matches!(stage_of("f2l"), Ok(CfopStage::F2l)));
        assert!(stage_of("x").is_err());
    }
}
