use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_core::{Alg, Cube, Face};
use bt_data::seedpack::SeedRow;
use std::collections::HashMap;

const POSITION_COUNT: usize = 54;
const PIECE_BAND_END: usize = 48;

pub fn map_wide_suffix(text: &str) -> String {
    text.split_whitespace()
        .map(|token| {
            let bytes = token.as_bytes();
            if bytes.len() >= 2
                && matches!(bytes[0], b'U' | b'L' | b'F' | b'R' | b'B' | b'D')
                && bytes[1] == b'w'
            {
                let mut mapped = String::with_capacity(token.len() - 1);
                mapped.push(bytes[0].to_ascii_lowercase() as char);
                mapped.push_str(&token[2..]);
                mapped
            } else {
                token.to_string()
            }
        })
        .collect::<Vec<String>>()
        .join(" ")
}

pub struct Layout {
    pieces: Vec<Vec<u8>>,
    masks: Vec<u8>,
    by_name: HashMap<u8, usize>,
}

fn face_bit(face: Face) -> u8 {
    1 << face.index()
}

fn faces_of(name: &str) -> Result<u8, String> {
    let mut mask = 0u8;
    for ch in name.chars() {
        let face = match ch {
            'U' => Face::U,
            'L' => Face::L,
            'F' => Face::F,
            'R' => Face::R,
            'B' => Face::B,
            'D' => Face::D,
            _ => return Err(format!("piece name {name} holds a non-face letter")),
        };
        mask |= face_bit(face);
    }
    Ok(mask)
}

impl Layout {
    pub fn probe() -> Layout {
        let mut membership = [0u8; POSITION_COUNT];
        for face in Face::ALL {
            let mut cube = Cube::solved();
            let mv = Alg::parse(&face_char(face)).expect("face turn parses");
            cube.apply_alg(&mv);
            for sticker in 0..POSITION_COUNT as u8 {
                if cube.position_of(sticker) != sticker {
                    membership[sticker as usize] |= face_bit(face);
                }
            }
        }
        let mut grouped: HashMap<u8, Vec<u8>> = HashMap::new();
        for (position, mask) in membership.iter().enumerate() {
            if position < PIECE_BAND_END {
                grouped.entry(*mask).or_default().push(position as u8);
            }
        }
        let mut pieces = Vec::new();
        let mut masks = Vec::new();
        let mut by_name = HashMap::new();
        for (mask, mut slot) in grouped {
            slot.sort_unstable();
            by_name.insert(mask, pieces.len());
            pieces.push(slot);
            masks.push(mask);
        }
        Layout {
            pieces,
            masks,
            by_name,
        }
    }

    pub fn slot_index(&self, name: &str) -> Result<usize, String> {
        let name = name.split('[').next().unwrap_or(name);
        let mask = faces_of(name)?;
        if mask.count_ones() != name.chars().count() as u32 {
            return Err(format!("piece name {name} repeats a face"));
        }
        self.by_name
            .get(&mask)
            .copied()
            .ok_or_else(|| format!("piece name {name} matches no slot"))
    }

    pub fn slot(&self, name: &str) -> Result<&[u8], String> {
        Ok(&self.pieces[self.slot_index(name)?])
    }

    pub fn corner_pieces(&self) -> Vec<usize> {
        self.band(3)
    }

    pub fn edge_pieces(&self) -> Vec<usize> {
        self.band(2)
    }

    fn band(&self, layers: u32) -> Vec<usize> {
        self.masks
            .iter()
            .enumerate()
            .filter(|(_, mask)| mask.count_ones() == layers)
            .map(|(index, _)| index)
            .collect()
    }
}

fn mask_name(mask: u8) -> String {
    let mut name = String::new();
    for face in Face::ALL {
        if mask & face_bit(face) != 0 {
            name.push(face_char(face).chars().next().expect("face char"));
        }
    }
    name
}

fn face_char(face: Face) -> String {
    match face {
        Face::U => "U".to_string(),
        Face::L => "L".to_string(),
        Face::F => "F".to_string(),
        Face::R => "R".to_string(),
        Face::B => "B".to_string(),
        Face::D => "D".to_string(),
    }
}

fn slot_set(state: &Cube, slot: &[u8]) -> Vec<u8> {
    let mut stickers: Vec<u8> = slot
        .iter()
        .map(|position| state.sticker_at(*position))
        .collect();
    stickers.sort_unstable();
    stickers
}

fn home_set(slot: &[u8]) -> Vec<u8> {
    let mut home = slot.to_vec();
    home.sort_unstable();
    home
}

pub struct StateAudit {
    pub piece_at: Vec<usize>,
    pub disturbed: Vec<u8>,
}

pub fn audit(state: &Cube, layout: &Layout) -> StateAudit {
    let home_sets: Vec<Vec<u8>> = layout.pieces.iter().map(|slot| home_set(slot)).collect();
    let mut piece_at = Vec::with_capacity(layout.pieces.len());
    for slot in &layout.pieces {
        let current = slot_set(state, slot);
        let index = home_sets
            .iter()
            .position(|home| *home == current)
            .expect("a legal state holds whole pieces");
        piece_at.push(index);
    }
    let disturbed = (0..POSITION_COUNT as u8)
        .filter(|position| state.sticker_at(*position) != *position)
        .collect();
    StateAudit {
        piece_at,
        disturbed,
    }
}

fn displaced(audit: &StateAudit, band: &[usize]) -> Vec<usize> {
    band.iter()
        .filter(|piece| audit.piece_at[**piece] != **piece)
        .copied()
        .collect()
}

fn confined(audit: &StateAudit, layout: &Layout, names: &[&str]) -> Result<(), String> {
    let mut allowed: Vec<u8> = Vec::new();
    for name in names {
        allowed.extend(layout.slot(name)?.iter().copied());
    }
    for position in &audit.disturbed {
        if !allowed.contains(position) {
            return Err(format!(
                "sticker at position {position} moved outside the named slots"
            ));
        }
    }
    Ok(())
}

fn zero_permutation(audit: &StateAudit, layout: &Layout) -> bool {
    (0..layout.pieces.len()).all(|piece| audit.piece_at[piece] == piece)
}

fn transposed(
    audit: &StateAudit,
    layout: &Layout,
    first: &str,
    second: &str,
) -> Result<(), String> {
    let a = layout.slot_index(first)?;
    let b = layout.slot_index(second)?;
    if audit.piece_at[a] != b || audit.piece_at[b] != a {
        return Err(format!("{first} and {second} are not transposed"));
    }
    Ok(())
}

fn others_home(
    audit: &StateAudit,
    layout: &Layout,
    band: &[usize],
    skip: &[usize],
) -> Result<(), String> {
    for piece in band {
        if skip.contains(piece) {
            continue;
        }
        if audit.piece_at[*piece] != *piece {
            let name = mask_name(layout.masks[*piece]);
            return Err(format!("piece {name} left its slot"));
        }
    }
    Ok(())
}

fn case_cube(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&alg.inverse());
    cube
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

fn fields(front_json: &str) -> Result<serde_json::Value, String> {
    serde_json::from_str(front_json).map_err(|err| err.to_string())
}

fn text_field(value: &serde_json::Value, key: &str) -> Option<String> {
    value
        .get(key)
        .and_then(|field| field.as_str())
        .map(|field| field.to_string())
}

fn parse_pair(spec: &str, separator: char) -> Result<[String; 2], String> {
    let parts: Vec<&str> = spec.split(separator).map(str::trim).collect();
    if parts.len() != 2 {
        return Err(format!("pair spec {spec} does not hold two names"));
    }
    Ok([parts[0].to_string(), parts[1].to_string()])
}

pub fn verify_oldpoch(rows: &[SeedRow]) -> SectionResult {
    let layout = Layout::probe();
    let mut outcomes = Vec::with_capacity(rows.len());
    let mut honesty = Vec::new();
    for row in rows {
        let tier1 = if row.alg.is_empty() {
            honesty.push(format!(
                "{} holds a prose alg, no mechanical verification exists",
                row.case_key
            ));
            Err("prose row holds no mechanical alg".to_string())
        } else {
            Alg::parse(&map_wide_suffix(&row.alg))
                .map_err(|err| err.to_string())
                .and_then(|alg| round_trip(&alg))
        };
        let is_letter = text_field(
            &fields(&row.front_json).unwrap_or(serde_json::Value::Null),
            "letter",
        )
        .is_some();
        if is_letter {
            honesty.push(format!(
                "{} is a letter scheme setup row, the letter to piece map is scheme knowledge",
                row.case_key
            ));
        }
        let tier2 = match (&tier1, is_letter) {
            (Err(_), _) => Tier2Outcome::Skipped("tier 1 failed"),
            (Ok(()), true) => Tier2Outcome::Skipped("letter scheme rows have no predicate"),
            (Ok(()), false) => {
                let alg = Alg::parse(&map_wide_suffix(&row.alg)).expect("tier 1 parsed the alg");
                let state = case_cube(&alg);
                let audit = audit(&state, &layout);
                match oldpoch_case(&row.front_json, &audit, &layout, &state) {
                    Ok(()) => Tier2Outcome::Pass,
                    Err(err) => Tier2Outcome::Fail(err),
                }
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
        honesty,
    }
}

fn oldpoch_case(
    front_json: &str,
    audit: &StateAudit,
    layout: &Layout,
    state: &Cube,
) -> Result<(), String> {
    let fields = fields(front_json)?;
    let kind = text_field(&fields, "kind");
    let swaps = text_field(&fields, "swaps");
    if kind.as_deref() == Some("parity")
        && let Some(swaps) = swaps.as_deref()
    {
        let (edge_pair, corner_pair) = parity_swaps(swaps)?;
        return double_transposition(audit, layout, &edge_pair, &corner_pair);
    }
    match kind.as_deref() {
        Some("parity") => {
            let edges = displaced(audit, &layout.edge_pieces());
            let corners = displaced(audit, &layout.corner_pieces());
            if edges.len() != 2 || corners.len() != 2 {
                return Err(format!(
                    "parity case displaces {} edges and {} corners, expected 2 and 2",
                    edges.len(),
                    corners.len()
                ));
            }
            Ok(())
        }
        Some("corner twist") => {
            let twists = text_field(&fields, "twists")
                .ok_or_else(|| "corner twist row names no twists".to_string())?;
            let names: Vec<String> = twists.split('+').map(str::to_string).collect();
            let names: Vec<&str> = names.iter().map(String::as_str).collect();
            confined(audit, layout, &names)?;
            if !zero_permutation(audit, layout) {
                return Err("corner twist row permutes pieces".to_string());
            }
            if state.is_solved() {
                return Err("twist row leaves the cube solved".to_string());
            }
            Ok(())
        }
        Some("edge swap") | Some("corner swap") | Some("setup example") => {
            let buffer = text_field(&fields, "buffer")
                .ok_or_else(|| "swap row names no buffer".to_string())?;
            let target = text_field(&fields, "target")
                .ok_or_else(|| "swap row names no target".to_string())?;
            transposed(audit, layout, &buffer, &target)?;
            let band = if buffer.chars().count() == 2 {
                layout.edge_pieces()
            } else {
                layout.corner_pieces()
            };
            others_home(
                audit,
                layout,
                &band,
                &[layout.slot_index(&buffer)?, layout.slot_index(&target)?],
            )
        }
        Some(other) => Err(format!("row kind {other} has no predicate")),
        None => {
            if text_field(&fields, "letter").is_some() {
                Err("letter scheme rows have no state predicate".to_string())
            } else {
                Err("row names neither kind nor letter".to_string())
            }
        }
    }
}

fn double_transposition(
    audit: &StateAudit,
    layout: &Layout,
    edge_pair: &[String; 2],
    corner_pair: &[String; 2],
) -> Result<(), String> {
    transposed(audit, layout, &edge_pair[0], &edge_pair[1])?;
    transposed(audit, layout, &corner_pair[0], &corner_pair[1])?;
    let band: Vec<usize> = layout
        .edge_pieces()
        .into_iter()
        .chain(layout.corner_pieces())
        .collect();
    let skip = vec![
        layout.slot_index(&edge_pair[0])?,
        layout.slot_index(&edge_pair[1])?,
        layout.slot_index(&corner_pair[0])?,
        layout.slot_index(&corner_pair[1])?,
    ];
    others_home(audit, layout, &band, &skip)
}

fn parity_swaps(swaps: &str) -> Result<([String; 2], [String; 2]), String> {
    let parts: Vec<&str> = swaps.split('+').map(str::trim).collect();
    if parts.len() != 2 {
        return Err(format!("parity spec {swaps} does not hold two pairs"));
    }
    let edge = parse_pair(parts[0], '-')?;
    let corner = parse_pair(parts[1], '-')?;
    Ok((edge, corner))
}

pub fn verify_m2(rows: &[SeedRow]) -> SectionResult {
    let layout = Layout::probe();
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = Alg::parse(&map_wide_suffix(&row.alg))
            .map_err(|err| err.to_string())
            .and_then(|alg| round_trip(&alg));
        let tier2 = match &tier1 {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok(()) => {
                let alg = Alg::parse(&map_wide_suffix(&row.alg)).expect("tier 1 parsed the alg");
                let state = case_cube(&alg);
                let audit = audit(&state, &layout);
                match m2_case(&row.front_json, &audit, &layout, &state) {
                    Ok(()) => Tier2Outcome::Pass,
                    Err(err) => Tier2Outcome::Fail(err),
                }
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

fn m2_case(
    front_json: &str,
    audit: &StateAudit,
    layout: &Layout,
    state: &Cube,
) -> Result<(), String> {
    let fields = fields(front_json)?;
    match text_field(&fields, "kind").as_deref() {
        Some("parity") => {
            let edges = displaced(audit, &layout.edge_pieces());
            let corners = displaced(audit, &layout.corner_pieces());
            if (edges.len() != 2 && edges.len() != 4) || !corners.is_empty() {
                return Err(format!(
                    "m2 parity case displaces {} edges and {} corners, expected 2 or 4 edges and no corners",
                    edges.len(),
                    corners.len()
                ));
            }
            Ok(())
        }
        Some("buffer edge flip") => {
            let flips = text_field(&fields, "flips")
                .ok_or_else(|| "flip row names no flips".to_string())?;
            let names: Vec<String> = flips.split('+').map(str::to_string).collect();
            let names: Vec<&str> = names.iter().map(String::as_str).collect();
            confined(audit, layout, &names)?;
            if !zero_permutation(audit, layout) {
                return Err("flip row permutes pieces".to_string());
            }
            if state.is_solved() {
                return Err("flip row leaves the cube solved".to_string());
            }
            Ok(())
        }
        _ => {
            let buffer = text_field(&fields, "buffer")
                .ok_or_else(|| "m2 row names no buffer".to_string())?;
            let target = text_field(&fields, "target")
                .ok_or_else(|| "m2 row names no target".to_string())?;
            let buffer_index = layout.slot_index(&buffer)?;
            let target_index = layout.slot_index(&target)?;
            if audit.piece_at[buffer_index] != target_index {
                return Err(format!("{target} does not land in the {buffer} buffer"));
            }
            let displaced = displaced(audit, &layout.edge_pieces());
            if displaced.len() > 4 {
                return Err(format!(
                    "m2 execution displaces {} edges, expected at most 4",
                    displaced.len()
                ));
            }
            Ok(())
        }
    }
}

pub fn verify_3style(rows: &[SeedRow]) -> SectionResult {
    let layout = Layout::probe();
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = Alg::parse(&map_wide_suffix(&row.alg))
            .map_err(|err| err.to_string())
            .and_then(|alg| round_trip(&alg));
        let tier2 = match &tier1 {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok(()) => {
                let alg = Alg::parse(&map_wide_suffix(&row.alg)).expect("tier 1 parsed the alg");
                let state = case_cube(&alg);
                let audit = audit(&state, &layout);
                match style_case(&row.front_json, &audit, &layout, &state) {
                    Ok(()) => Tier2Outcome::Pass,
                    Err(err) => Tier2Outcome::Fail(err),
                }
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

fn style_case(
    front_json: &str,
    audit: &StateAudit,
    layout: &Layout,
    state: &Cube,
) -> Result<(), String> {
    let fields = fields(front_json)?;
    let position =
        text_field(&fields, "position").ok_or_else(|| "style row names no position".to_string())?;
    if position.contains(" & ") {
        let pair = parse_pair(&position, '&')?;
        let names: Vec<&str> = vec![pair[0].as_str(), pair[1].as_str()];
        confined(audit, layout, &names)?;
        if !zero_permutation(audit, layout) {
            return Err(format!("{position} row permutes pieces"));
        }
        if state.is_solved() {
            return Err(format!("{position} row leaves the cube solved"));
        }
        return Ok(());
    }
    let parts: Vec<&str> = position.split(',').map(str::trim).collect();
    if parts.len() == 2 {
        let edge = parse_pair(parts[0], '-')?;
        let corner = parse_pair(parts[1], '-')?;
        return double_transposition(audit, layout, &edge, &corner);
    }
    Err(format!(
        "position {position} names neither a pair nor two swaps"
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn wide_suffix_tokens_map_to_lowercase_wide_moves() {
        assert_eq!(map_wide_suffix("Lw' D L2"), "l' D L2");
        assert_eq!(map_wide_suffix("Dw2 L"), "d2 L");
        assert_eq!(map_wide_suffix("R U R'"), "R U R'");
    }

    #[test]
    fn the_probe_yields_eight_corners_twelve_edges_and_named_slots() {
        let layout = Layout::probe();
        assert_eq!(layout.corner_pieces().len(), 8);
        assert_eq!(layout.edge_pieces().len(), 12);
        assert_eq!(layout.slot("UFR").expect("corner slot").len(), 3);
        assert_eq!(layout.slot("UR").expect("edge slot").len(), 2);
        assert!(layout.slot_index("UD").is_err());
        assert!(layout.slot_index("UFRF").is_err());
    }

    #[test]
    fn solved_cube_audits_with_no_disturbance() {
        let layout = Layout::probe();
        let audit = audit(&Cube::solved(), &layout);
        assert!(audit.disturbed.is_empty());
        assert!(zero_permutation(&audit, &layout));
    }

    #[test]
    fn a_t_perm_case_state_transposes_the_named_edges() {
        let layout = Layout::probe();
        let alg = Alg::parse(&map_wide_suffix("R U R' U' R' F R2 U' R' U' R U R' F'"))
            .expect("t perm parses");
        let state = case_cube(&alg);
        let audit = audit(&state, &layout);
        assert!(transposed(&audit, &layout, "UR", "UL").is_ok());
        assert!(transposed(&audit, &layout, "UR", "UF").is_err());
    }

    #[test]
    fn plain_m2_transposes_df_and_ub_with_four_displaced_edges() {
        let layout = Layout::probe();
        let alg = Alg::parse("M2").expect("m2 parses");
        let state = case_cube(&alg);
        let audit = audit(&state, &layout);
        assert!(transposed(&audit, &layout, "DF", "UB").is_ok());
        assert_eq!(displaced(&audit, &layout.edge_pieces()).len(), 4);
    }

    #[test]
    fn pair_and_swap_specs_parse() {
        assert_eq!(
            parse_pair("UF & UB", '&'),
            Ok(["UF".to_string(), "UB".to_string()])
        );
        let (edge, corner) = parity_swaps("UF-UR+UFR-UBR").expect("parity parses");
        assert_eq!(edge[0], "UF");
        assert_eq!(corner[1], "UBR");
        assert!(parity_swaps("UF-UR").is_err());
    }

    #[test]
    fn corner_names_match_slots_in_any_letter_order() {
        let layout = Layout::probe();
        assert_eq!(layout.slot_index("UFR"), layout.slot_index("URF"));
        assert_eq!(layout.slot_index("UR"), layout.slot_index("RU"));
        assert_ne!(layout.slot_index("UFR"), layout.slot_index("UFL"));
    }
}
