use crate::geom;
use crate::walk::Env;
use bt_scheme::{Classification, Orbit};

pub struct Row {
    pub solver: String,
    pub sheet: String,
    pub rowid: String,
    pub position: String,
    pub moves: String,
    pub qtm: String,
    pub matchv: String,
    pub note: String,
    pub occ: [u8; 54],
}

pub fn parse_simocc(text: &str) -> Vec<Row> {
    let mut rows = Vec::new();
    for line in text.lines() {
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 9, "simocc row width");
        let occ: [u8; 54] = cols[8]
            .split(',')
            .map(|v| v.parse().unwrap())
            .collect::<Vec<u8>>()
            .try_into()
            .unwrap();
        rows.push(Row {
            solver: cols[0].to_string(),
            sheet: cols[1].to_string(),
            rowid: cols[2].to_string(),
            position: cols[3].to_string(),
            moves: cols[4].to_string(),
            qtm: cols[5].to_string(),
            matchv: cols[6].to_string(),
            note: cols[7].to_string(),
            occ,
        });
    }
    rows
}

pub fn sticker_of_name(name: &str) -> u8 {
    for s in 0..48u8 {
        if bt_scheme::name_of(s) == name {
            return s;
        }
    }
    panic!("unknown sticker name {name}");
}

pub fn group_of_sticker_name(name: &str) -> usize {
    let s = sticker_of_name(name);
    if s < 24 {
        geom::corner_of_sticker(s)
    } else {
        geom::edge_of_sticker(s)
    }
}

#[derive(Default, Debug)]
pub struct NoteDefects {
    pub edges: Vec<(String, bool)>,
    pub corners: Vec<(String, i8)>,
    pub twist_pair: Option<((String, i8), (String, i8))>,
}

fn int_of(s: &str) -> i8 {
    s.parse().unwrap()
}

pub fn parse_note(sheet: &str, note: &str) -> NoteDefects {
    let mut out = NoteDefects::default();
    if note.is_empty() {
        return out;
    }
    match sheet {
        "edge" | "corner" => {
            let body = note
                .strip_prefix("defects:")
                .unwrap_or(note)
                .trim_end_matches("clean")
                .trim();
            for item in body.split(',') {
                let (name, val) = item.split_once('=').unwrap();
                if sheet == "edge" {
                    out.edges.push((name.to_string(), val == "flip"));
                } else {
                    out.corners.push((name.to_string(), int_of(val)));
                }
            }
        }
        "parity" => {
            let body = note
                .strip_prefix("defects:")
                .unwrap_or(note)
                .trim_end_matches("clean")
                .trim();
            for (idx, section) in body.split_whitespace().enumerate() {
                for item in section.split(',') {
                    let (name, val) = item.split_once('=').unwrap();
                    if idx == 0 {
                        out.edges.push((name.to_string(), val == "flip"));
                    } else {
                        out.corners.push((name.to_string(), int_of(val)));
                    }
                }
            }
        }
        "twists" => {
            let p1 = note.split('[').nth(1).unwrap();
            let name1 = p1.split('>').next().unwrap();
            let p2 = note.split('[').nth(2).unwrap();
            let name2 = p2.split('>').next().unwrap();
            let d1 = note.split("d1=").nth(1).unwrap().split(' ').next().unwrap();
            let d2 = note.split("d2=").nth(1).unwrap().split(' ').next().unwrap();
            out.twist_pair = Some((
                (name1.to_string(), int_of(d1)),
                (name2.to_string(), int_of(d2)),
            ));
        }
        _ => {}
    }
    out
}

pub fn note_clean(sheet: &str, note: &str) -> bool {
    match sheet {
        "flips" => true,
        "twists" | "ltct" => false,
        _ => note.ends_with("clean"),
    }
}

fn same_group(a: u8, b: u8) -> bool {
    if a < 24 {
        (b < 24) && geom::corner_of_sticker(a) == geom::corner_of_sticker(b)
    } else {
        (b >= 24) && geom::edge_of_sticker(a) == geom::edge_of_sticker(b)
    }
}

pub fn my_trace(occ: &[u8; 54], orbit: Orbit, env: &Env) -> Vec<Vec<u8>> {
    let corner = orbit == Orbit::Corner;
    let mut visited = [false; 48];
    let mut blocks = Vec::new();
    for &g in env.floating(orbit) {
        let head = geom::head_of_group(corner, g);
        let members = if corner {
            geom::CORNER_GROUPS[g].to_vec()
        } else {
            geom::EDGE_GROUPS[g].to_vec()
        };
        if members.iter().any(|&s| visited[s as usize]) {
            continue;
        }
        let mut block = vec![head];
        while occ[block[block.len() - 1] as usize] != head {
            block.push(occ[block[block.len() - 1] as usize]);
        }
        if block.len() > 1 {
            block.push(head);
            for &s in &block {
                visited[s as usize] = true;
            }
            blocks.push(block);
        }
    }
    blocks
}

fn is_style(b: &[u8]) -> bool {
    b.len() == 4 && !same_group(b[0], b[1]) && !same_group(b[0], b[2]) && !same_group(b[1], b[2])
}

fn is_flip(b: &[u8]) -> bool {
    b.len() == 3 && same_group(b[0], b[1])
}

fn is_swap(b: &[u8]) -> bool {
    b.len() == 3 && !same_group(b[0], b[1])
}

fn is_twist(b: &[u8]) -> bool {
    b.len() == 4 && same_group(b[0], b[1]) && same_group(b[0], b[2]) && same_group(b[1], b[2])
}

fn is_swap_twist(b: &[u8]) -> bool {
    b.len() == 7
        && (0..3).all(|i| same_group(b[2 * i], b[0]))
        && (0..3).all(|i| same_group(b[2 * i + 1], b[1]))
        && !same_group(b[0], b[1])
}

pub fn my_classify(edge: &[Vec<u8>], corner: &[Vec<u8>]) -> String {
    let edge_solved = edge.is_empty();
    let corner_solved = corner.is_empty();
    if edge_solved && corner_solved {
        return "Solved".to_string();
    }
    if corner_solved && edge.len() == 1 && is_style(&edge[0]) {
        return "ThreeStyle(Edge)".to_string();
    }
    if edge_solved && corner.len() == 1 && is_style(&corner[0]) {
        return "ThreeStyle(Corner)".to_string();
    }
    if corner_solved && edge.len() == 2 && is_flip(&edge[0]) && is_flip(&edge[1]) {
        return "FlipPair".to_string();
    }
    if edge_solved && corner.len() == 2 && is_twist(&corner[0]) && is_twist(&corner[1]) {
        return "TwistPair".to_string();
    }
    if edge.len() == 1 && is_swap(&edge[0]) {
        if corner.len() == 1 && is_swap(&corner[0]) {
            return "Parity".to_string();
        }
        if corner.len() == 2 {
            let ordered = if is_swap_twist(&corner[0]) && is_twist(&corner[1]) {
                true
            } else {
                is_twist(&corner[0]) && is_swap_twist(&corner[1])
            };
            if ordered {
                return "Ltct".to_string();
            }
        }
    }
    "Unclassified".to_string()
}

pub fn class_tag(class: &Classification) -> String {
    match class {
        Classification::Solved => "Solved".to_string(),
        Classification::ThreeStyle { orbit, .. } => format!("ThreeStyle({orbit:?})"),
        Classification::FlipPair { .. } => "FlipPair".to_string(),
        Classification::TwistPair { .. } => "TwistPair".to_string(),
        Classification::Parity { .. } => "Parity".to_string(),
        Classification::Ltct { .. } => "Ltct".to_string(),
        Classification::Unclassified(_) => "Unclassified".to_string(),
    }
}

pub fn expected_sheet_tag(sheet: &str, clean: bool, my_tag: &str) -> String {
    match sheet {
        "flips" => "FlipPair".to_string(),
        "twists" => "TwistPair".to_string(),
        _ if clean => match sheet {
            "edge" => "ThreeStyle(Edge)".to_string(),
            "corner" => "ThreeStyle(Corner)".to_string(),
            "parity" => "Parity".to_string(),
            "ltct" => "Ltct".to_string(),
            _ => my_tag.to_string(),
        },
        _ => my_tag.to_string(),
    }
}
