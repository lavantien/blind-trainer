pub const FACES: [char; 6] = ['U', 'L', 'F', 'R', 'B', 'D'];
pub const NORMALS: [[i8; 3]; 6] = [
    [0, 1, 0],
    [-1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];

const fn grid(face: usize, r: i32, c: i32) -> [i32; 3] {
    match face {
        0 => [c - 1, 1, r - 1],
        1 => [-1, 1 - r, c - 1],
        2 => [c - 1, 1 - r, 1],
        3 => [1, 1 - r, 1 - c],
        4 => [1 - c, 1 - r, -1],
        _ => [c - 1, -1, 1 - r],
    }
}

const fn home_cell(id: usize) -> (usize, i32, i32) {
    if id < 24 {
        let pos = match id % 4 {
            0 => (0, 0),
            1 => (0, 2),
            2 => (2, 2),
            _ => (2, 0),
        };
        (id / 4, pos.0, pos.1)
    } else if id < 48 {
        let e = id - 24;
        let pos = match e % 4 {
            0 => (0, 1),
            1 => (1, 2),
            2 => (2, 1),
            _ => (1, 0),
        };
        (e / 4, pos.0, pos.1)
    } else {
        (id - 48, 1, 1)
    }
}

const fn home_pos(id: usize) -> [i32; 3] {
    let (f, r, c) = home_cell(id);
    grid(f, r, c)
}

const fn same_pos(a: [i32; 3], b: [i32; 3]) -> bool {
    a[0] == b[0] && a[1] == b[1] && a[2] == b[2]
}

const fn build_corner_groups() -> [[u8; 3]; 8] {
    let mut groups = [[0u8; 3]; 8];
    let mut count = 0usize;
    let mut id = 0usize;
    while id < 24 {
        let mut dup = false;
        let mut j = 0usize;
        while j < id {
            if same_pos(home_pos(id), home_pos(j)) {
                dup = true;
            }
            j += 1;
        }
        if !dup {
            let p = home_pos(id);
            let mut members = [id as u8, 0u8, 0u8];
            let mut filled = 1usize;
            let mut k = id + 1;
            while k < 24 {
                if same_pos(p, home_pos(k)) {
                    members[filled] = k as u8;
                    filled += 1;
                }
                k += 1;
            }
            groups[count] = members;
            count += 1;
        }
        id += 1;
    }
    groups
}

const fn build_edge_groups() -> [[u8; 2]; 12] {
    let mut groups = [[0u8; 2]; 12];
    let mut count = 0usize;
    let mut id = 24usize;
    while id < 48 {
        let mut dup = false;
        let mut j = 24usize;
        while j < id {
            if same_pos(home_pos(id), home_pos(j)) {
                dup = true;
            }
            j += 1;
        }
        if !dup {
            let p = home_pos(id);
            let mut members = [id as u8, 0u8];
            let mut filled = 1usize;
            let mut k = id + 1;
            while k < 48 {
                if same_pos(p, home_pos(k)) {
                    members[filled] = k as u8;
                    filled += 1;
                }
                k += 1;
            }
            groups[count] = members;
            count += 1;
        }
        id += 1;
    }
    groups
}

pub const CORNER_GROUPS: [[u8; 3]; 8] = build_corner_groups();
pub const EDGE_GROUPS: [[u8; 2]; 12] = build_edge_groups();

const fn build_corner_of() -> [u8; 24] {
    let mut table = [0u8; 24];
    let mut g = 0usize;
    while g < 8 {
        let mut i = 0;
        while i < 3 {
            table[CORNER_GROUPS[g][i] as usize] = g as u8;
            i += 1;
        }
        g += 1;
    }
    table
}

const fn build_edge_of() -> [u8; 24] {
    let mut table = [0u8; 24];
    let mut g = 0usize;
    while g < 12 {
        let mut i = 0;
        while i < 2 {
            table[(EDGE_GROUPS[g][i] - 24) as usize] = g as u8;
            i += 1;
        }
        g += 1;
    }
    table
}

pub const CORNER_OF: [u8; 24] = build_corner_of();
pub const EDGE_OF: [u8; 24] = build_edge_of();

pub fn corner_of_sticker(s: u8) -> usize {
    CORNER_OF[s as usize] as usize
}

pub fn edge_of_sticker(s: u8) -> usize {
    EDGE_OF[(s - 24) as usize] as usize
}

pub fn face_of_sticker(s: u8) -> usize {
    if s < 24 {
        (s / 4) as usize
    } else if s < 48 {
        ((s - 24) / 4) as usize
    } else {
        (s - 48) as usize
    }
}

pub fn face_set_of_group(corner: bool, g: usize) -> u8 {
    let mut mask = 0u8;
    if corner {
        for &s in &CORNER_GROUPS[g] {
            mask |= 1 << face_of_sticker(s);
        }
    } else {
        for &s in &EDGE_GROUPS[g] {
            mask |= 1 << face_of_sticker(s);
        }
    }
    mask
}

pub fn face_mask_of_name(name: &str) -> u8 {
    let mut mask = 0u8;
    for ch in name.chars() {
        mask |= 1 << FACES.iter().position(|&f| f == ch).unwrap();
    }
    mask
}

pub fn corner_group_of_name(name: &str) -> usize {
    let mask = face_mask_of_name(name);
    (0..8).find(|&g| face_set_of_group(true, g) == mask).unwrap()
}

pub fn edge_group_of_name(name: &str) -> usize {
    let mask = face_mask_of_name(name);
    (0..12).find(|&g| face_set_of_group(false, g) == mask).unwrap()
}

pub fn head_of_group(corner: bool, g: usize) -> u8 {
    let members: &[u8] = if corner {
        &CORNER_GROUPS[g]
    } else {
        &EDGE_GROUPS[g]
    };
    for &s in members {
        let f = FACES[face_of_sticker(s)];
        if f == 'U' || f == 'D' {
            return s;
        }
    }
    for &s in members {
        let f = FACES[face_of_sticker(s)];
        if f == 'F' || f == 'B' {
            return s;
        }
    }
    members[0]
}

pub fn is_bijective(occ: &[u8; 54]) -> bool {
    let mut seen = [false; 54];
    for &s in occ.iter() {
        if s >= 54 || seen[s as usize] {
            return false;
        }
        seen[s as usize] = true;
    }
    true
}

pub fn centers_permute(occ: &[u8; 54]) -> bool {
    let mut seen = [false; 6];
    for i in 48..54 {
        let s = occ[i];
        if s < 48 || s >= 54 || seen[(s - 48) as usize] {
            return false;
        }
        seen[(s - 48) as usize] = true;
    }
    true
}

pub fn cohesive(occ: &[u8; 54]) -> bool {
    for g in 0..8 {
        let members = CORNER_GROUPS[g];
        if occ[members[0] as usize] >= 24 {
            return false;
        }
        let target = corner_of_sticker(occ[members[0] as usize]);
        for &m in &members[1..] {
            if occ[m as usize] >= 24 || corner_of_sticker(occ[m as usize]) != target {
                return false;
            }
        }
    }
    for g in 0..12 {
        let members = EDGE_GROUPS[g];
        let first = occ[members[0] as usize];
        if first < 24 || first >= 48 {
            return false;
        }
        let target = edge_of_sticker(first);
        for &m in &members[1..] {
            let o = occ[m as usize];
            if o < 24 || o >= 48 || edge_of_sticker(o) != target {
                return false;
            }
        }
    }
    true
}

fn cross(a: [i8; 3], b: [i8; 3]) -> [i8; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn dot(a: [i8; 3], b: [i8; 3]) -> i32 {
    a.iter().zip(b.iter()).map(|(x, y)| *x as i32 * *y as i32).sum()
}

pub fn corner_twist(occ: &[u8; 54], g: usize) -> u8 {
    let members = CORNER_GROUPS[g];
    let mut ud_cell = None;
    let mut slot_ud_cell = None;
    for &m in &members {
        let f = FACES[face_of_sticker(m)];
        if f == 'U' || f == 'D' {
            slot_ud_cell = Some(m);
        }
        let of = FACES[face_of_sticker(occ[m as usize])];
        if of == 'U' || of == 'D' {
            ud_cell = Some(m);
        }
    }
    let ud = ud_cell.expect("corner occupant has no ud sticker");
    let slot_ud = slot_ud_cell.expect("corner slot has no ud face");
    if ud == slot_ud {
        return 0;
    }
    let nu = NORMALS[face_of_sticker(slot_ud)];
    let mut d = [0i8; 3];
    for &m in &members {
        let n = NORMALS[face_of_sticker(m)];
        d[0] += n[0];
        d[1] += n[1];
        d[2] += n[2];
    }
    let others: Vec<u8> = members
        .iter()
        .copied()
        .filter(|&m| m != slot_ud)
        .collect();
    let na = NORMALS[face_of_sticker(others[0])];
    let first = if dot(cross(nu, na), d) > 0 { 0 } else { 1 };
    let target = others[first];
    if ud == target {
        1
    } else {
        2
    }
}

fn axis_priority(face: usize) -> u8 {
    match FACES[face] {
        'U' | 'D' => 2,
        'F' | 'B' => 1,
        _ => 0,
    }
}

pub fn edge_flip(occ: &[u8; 54], g: usize) -> bool {
    let members = EDGE_GROUPS[g];
    let f1 = face_of_sticker(members[0]);
    let f2 = face_of_sticker(members[1]);
    let c1 = (occ[48 + f1] - 48) as usize;
    let c2 = (occ[48 + f2] - 48) as usize;
    let slot_high = axis_priority(c1) > axis_priority(c2);
    let t1 = occ[members[0] as usize];
    let t2 = occ[members[1] as usize];
    let piece_high = axis_priority(face_of_sticker(t1)) > axis_priority(face_of_sticker(t2));
    piece_high != slot_high
}

pub fn perm_sign(perm: &[usize]) -> i8 {
    let n = perm.len();
    let mut seen = vec![false; n];
    let mut sign = 1i8;
    for i in 0..n {
        if seen[i] {
            continue;
        }
        let mut len = 0usize;
        let mut cur = i;
        while !seen[cur] {
            seen[cur] = true;
            cur = perm[cur];
            len += 1;
        }
        if len % 2 == 0 {
            sign = -sign;
        }
    }
    sign
}

pub fn occupant_perm(occ: &[u8; 54], corner: bool) -> Option<Vec<usize>> {
    let n = if corner { 8 } else { 12 };
    let mut perm = Vec::with_capacity(n);
    for g in 0..n {
        let probe = if corner {
            CORNER_GROUPS[g][0]
        } else {
            EDGE_GROUPS[g][0]
        };
        let occ_sticker = occ[probe as usize];
        if corner {
            if occ_sticker >= 24 {
                return None;
            }
            perm.push(corner_of_sticker(occ_sticker));
        } else {
            if occ_sticker < 24 || occ_sticker >= 48 {
                return None;
            }
            perm.push(edge_of_sticker(occ_sticker));
        }
    }
    Some(perm)
}

pub fn twist_sum(occ: &[u8; 54]) -> u32 {
    (0..8).map(|g| corner_twist(occ, g) as u32).sum()
}

pub fn flip_count(occ: &[u8; 54]) -> u32 {
    (0..12).filter(|&g| edge_flip(occ, g)).count() as u32
}

pub fn edge_cell_roles(g: usize) -> [u8; 2] {
    let m = EDGE_GROUPS[g];
    let class = |s: u8| match FACES[face_of_sticker(s)] {
        'U' | 'D' => 0,
        'F' | 'B' => 1,
        _ => 2,
    };
    if class(m[0]) <= class(m[1]) {
        [m[0], m[1]]
    } else {
        [m[1], m[0]]
    }
}

pub fn corner_cell_roles(g: usize) -> [u8; 3] {
    let m = CORNER_GROUPS[g];
    let class = |s: u8| match FACES[face_of_sticker(s)] {
        'U' | 'D' => 0,
        'F' | 'B' => 1,
        _ => 2,
    };
    let mut ud = m[0];
    for &s in &m {
        if class(s) == 0 {
            ud = s;
        }
    }
    let others: Vec<u8> = m.iter().copied().filter(|&s| s != ud).collect();
    let nu = NORMALS[face_of_sticker(ud)];
    let mut d = [0i8; 3];
    for &s in &m {
        let n = NORMALS[face_of_sticker(s)];
        d[0] += n[0];
        d[1] += n[1];
        d[2] += n[2];
    }
    let na = NORMALS[face_of_sticker(others[0])];
    let cross = [
        nu[1] * na[2] - nu[2] * na[1],
        nu[2] * na[0] - nu[0] * na[2],
        nu[0] * na[1] - nu[1] * na[0],
    ];
    let dot = cross[0] as i32 * d[0] as i32
        + cross[1] as i32 * d[1] as i32
        + cross[2] as i32 * d[2] as i32;
    if dot > 0 {
        [ud, others[0], others[1]]
    } else {
        [ud, others[1], others[0]]
    }
}

pub fn placement_is_rotation(occ: &[u8; 54], slot: usize) -> bool {
    let cells = corner_cell_roles(slot);
    let piece = corner_of_sticker(occ[cells[0] as usize]);
    let stickers = corner_cell_roles(piece as usize);
    let w0 = stickers
        .iter()
        .position(|&s| s == occ[cells[0] as usize])
        .unwrap();
    for k in 0..3 {
        let want = stickers[(k + w0) % 3];
        if occ[cells[k] as usize] != want {
            return false;
        }
    }
    true
}

pub fn is_legal(occ: &[u8; 54]) -> bool {
    is_bijective(occ)
        && centers_permute(occ)
        && cohesive(occ)
        && (0..8).all(|g| placement_is_rotation(occ, g))
        && twist_sum(occ) % 3 == 0
        && flip_count(occ) % 2 == 0
        && match (occupant_perm(occ, true), occupant_perm(occ, false)) {
            (Some(c), Some(e)) => perm_sign(&c) == perm_sign(&e),
            _ => false,
        }
}

pub fn bt_corner_slot(my_slot: usize) -> u8 {
    let mine = CORNER_GROUPS[my_slot];
    for k in 0..8u8 {
        let s = bt_core::corner_stickers(k);
        if s.iter().all(|&x| mine.contains(&x)) && mine.iter().all(|&x| s.contains(&x)) {
            return k;
        }
    }
    panic!("no bt corner slot for {mine:?}");
}

pub fn bt_corner_head(my_piece: usize) -> u8 {
    let k = bt_corner_slot(my_piece);
    bt_core::head_sticker(bt_core::Piece::Corner(k))
}

pub fn slot_of_edge_piece(occ: &[u8; 54], piece: usize) -> usize {
    (0..12)
        .find(|&s| edge_of_sticker(occ[EDGE_GROUPS[s][0] as usize]) == piece)
        .unwrap()
}

pub fn slot_of_corner_piece(occ: &[u8; 54], piece: usize) -> usize {
    (0..8)
        .find(|&s| corner_of_sticker(occ[CORNER_GROUPS[s][0] as usize]) == piece)
        .unwrap()
}

pub fn in_place(occ: &[u8; 54], corner: bool, g: usize) -> bool {
    let members: &[u8] = if corner {
        &CORNER_GROUPS[g]
    } else {
        &EDGE_GROUPS[g]
    };
    let mut occupants: Vec<u8> = members.iter().map(|&m| occ[m as usize]).collect();
    let mut homes: Vec<u8> = members.to_vec();
    occupants.sort_unstable();
    homes.sort_unstable();
    occupants == homes
}

pub fn solved_group(occ: &[u8; 54], corner: bool, g: usize) -> bool {
    let members: &[u8] = if corner {
        &CORNER_GROUPS[g]
    } else {
        &EDGE_GROUPS[g]
    };
    members.iter().all(|&m| occ[m as usize] == m)
}
