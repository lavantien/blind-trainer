use std::f64::consts::PI;

const PHI: f64 = 1.618_033_988_749_895;
const STEP: f64 = 72.0;

fn vertices() -> [[f64; 3]; 12] {
    [
        [0.0, 1.0, PHI],
        [0.0, -1.0, PHI],
        [1.0, PHI, 0.0],
        [-1.0, PHI, 0.0],
        [PHI, 0.0, 1.0],
        [-PHI, 0.0, 1.0],
        [0.0, 1.0, -PHI],
        [0.0, -1.0, -PHI],
        [1.0, -PHI, 0.0],
        [-1.0, -PHI, 0.0],
        [PHI, 0.0, -1.0],
        [-PHI, 0.0, -1.0],
    ]
}

fn dot(a: [f64; 3], b: [f64; 3]) -> f64 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn cross(a: [f64; 3], b: [f64; 3]) -> [f64; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn scale(a: [f64; 3], k: f64) -> [f64; 3] {
    [a[0] * k, a[1] * k, a[2] * k]
}

fn add(a: [f64; 3], b: [f64; 3]) -> [f64; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

fn sub(a: [f64; 3], b: [f64; 3]) -> [f64; 3] {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}

fn neg(a: [f64; 3]) -> [f64; 3] {
    [-a[0], -a[1], -a[2]]
}

fn unit(a: [f64; 3]) -> [f64; 3] {
    scale(a, 1.0 / dot(a, a).sqrt())
}

fn rotate(v: [f64; 3], axis: [f64; 3], angle_rad: f64) -> [f64; 3] {
    let cos = angle_rad.cos();
    let sin = angle_rad.sin();
    add(
        add(scale(v, cos), scale(cross(axis, v), sin)),
        scale(axis, dot(axis, v) * (1.0 - cos)),
    )
}

fn dist2(a: [f64; 3], b: [f64; 3]) -> f64 {
    let d = sub(a, b);
    dot(d, d)
}

fn adjacency(verts: &[[f64; 3]; 12]) -> Vec<Vec<usize>> {
    (0..12)
        .map(|i| {
            (0..12)
                .filter(|&j| i != j && dist2(verts[i], verts[j]) < 9.0)
                .collect()
        })
        .collect()
}

fn nearest_of(target: [f64; 3], cands: &[usize], verts: &[[f64; 3]; 12]) -> usize {
    let mut best = cands[0];
    let mut best_d = dist2(target, verts[best]);
    for &c in &cands[1..] {
        let d = dist2(target, verts[c]);
        if d < best_d {
            best_d = d;
            best = c;
        }
    }
    best
}

fn ring_order(verts: &[[f64; 3]; 12], adj: &[Vec<usize>], f: usize, start: usize) -> Vec<usize> {
    let axis = unit(verts[f]);
    let mut order = Vec::new();
    let mut cur = start;
    for _ in 0..adj[f].len() {
        order.push(cur);
        let rotated = rotate(verts[cur], axis, -STEP * PI / 180.0);
        cur = nearest_of(rotated, &adj[f], verts);
    }
    assert_eq!(cur, start, "ring does not close for face {}", f);
    order
}

fn sigma_all(verts: &[[f64; 3]; 12], axis_face: usize, angle_deg: f64) -> Vec<usize> {
    let axis = unit(verts[axis_face]);
    let all: Vec<usize> = (0..12).collect();
    (0..12)
        .map(|j| nearest_of(rotate(verts[j], axis, angle_deg * PI / 180.0), &all, verts))
        .collect()
}

fn main() {
    let verts = vertices();
    let adj = adjacency(&verts);
    for (i, list) in adj.iter().enumerate() {
        assert_eq!(list.len(), 5, "face {} degree", i);
        for &j in list {
            assert!(adj[j].contains(&i), "asymmetry {} {}", i, j);
        }
    }

    let u = nearest_of([0.0, 1.0, PHI], &(0..12).collect::<Vec<_>>(), &verts);
    let f = nearest_of([PHI, 0.0, 1.0], &(0..12).collect::<Vec<_>>(), &verts);
    assert!(adj[u].contains(&f), "F not adjacent to U");
    let d = nearest_of(neg(verts[u]), &(0..12).collect::<Vec<_>>(), &verts);
    let b = nearest_of(neg(verts[f]), &(0..12).collect::<Vec<_>>(), &verts);

    let u_ring = ring_order(&verts, &adj, u, f);
    assert_eq!(u_ring[0], f, "canonical start of U ring must be F");
    let l = u_ring[1];
    let bl = u_ring[2];
    let br = u_ring[3];
    let r = u_ring[4];

    let top = [f, r, br, bl, l];
    let mut known = vec![u, d, f, b, l, r, br, bl];
    let mut bottom = Vec::new();
    for v in 0..12 {
        if !known.contains(&v) {
            let over: Vec<usize> = adj[v].iter().copied().filter(|x| top.contains(x)).collect();
            assert_eq!(over.len(), 2, "bottom face top pair {:?}", over);
            bottom.push((v, over));
        }
    }
    assert_eq!(bottom.len(), 4);

    let by_pair = |x: usize, y: usize| -> Option<usize> {
        bottom
            .iter()
            .find(|(_, pair)| (pair[0] == x && pair[1] == y) || (pair[0] == y && pair[1] == x))
            .map(|&(v, _)| v)
    };
    let ur = by_pair(r, br).expect("face under R and BR");
    let dr = by_pair(f, r).expect("face under F and R");
    let dl = by_pair(f, l).expect("face under F and L");
    let ul = by_pair(l, bl).expect("face under L and BL");
    known.extend([&ur, &dr, &dl, &ul]);
    assert_eq!(known.len(), 12);

    let antipode = |v: usize| nearest_of(neg(verts[v]), &(0..12).collect::<Vec<_>>(), &verts);
    for v in 0..12 {
        println!("antipode {} -> {}", v, antipode(v));
    }
    println!("pair under R BR: {}", antipode(ur));
    println!("pair under F L: {}", antipode(dl));
    println!("pair under F R: {}", antipode(dr));
    println!("pair under L BL: {}", antipode(ul));
    assert_eq!(antipode(f), b, "F opposite B");
    assert_eq!(antipode(u), d, "U opposite D");

    let ids = [u, f, r, br, bl, l, d, b, ur, dr, dl, ul];
    let names = [
        "U", "F", "R", "BR", "BL", "L", "D", "B", "UR", "DR", "DL", "UL",
    ];
    let id_of = |v: usize| ids.iter().position(|&x| x == v).unwrap();

    let mut corners = Vec::new();
    for i in 0..12 {
        for j in (i + 1)..12 {
            if !adj[i].contains(&j) {
                continue;
            }
            for k in (j + 1)..12 {
                if adj[i].contains(&k) && adj[j].contains(&k) {
                    corners.push((i, j, k));
                }
            }
        }
    }
    assert_eq!(corners.len(), 20, "dodecahedron corner count");
    let mut edge_count = 0;
    for i in 0..12 {
        edge_count += adj[i].len();
    }
    assert_eq!(edge_count, 30 * 2, "edge dart count");

    let mut table: Vec<Vec<usize>> = Vec::new();
    for slot in 0..12 {
        let face = ids[slot];
        let start = *adj[face].iter().min_by_key(|&&v| id_of(v)).unwrap();
        let order = ring_order(&verts, &adj, face, start);
        table.push(order.iter().map(|&v| id_of(v)).collect());
    }

    let sigma = sigma_all(&verts, u, -STEP);
    let y_perm: Vec<usize> = (0..12).map(|j| id_of(sigma[ids[j]])).collect();
    assert_eq!(y_perm[id_of(u)], id_of(u), "y fixes U");
    assert_eq!(y_perm[id_of(d)], id_of(d), "y fixes D");
    for (k, &n) in table[id_of(u)].iter().enumerate() {
        let expect = table[id_of(u)][(k + 1) % 5];
        assert_eq!(y_perm[n], expect, "y ring step at {}", k);
    }
    for g in 0..12 {
        let img: Vec<usize> = table[g].iter().map(|&n| y_perm[n]).collect();
        let mut shift = None;
        for t in 0..5 {
            let candidate: Vec<usize> = (0..5).map(|k| table[y_perm[g]][(k + t) % 5]).collect();
            if candidate == img {
                shift = Some(t);
            }
        }
        assert!(
            shift.is_some(),
            "y must be rotation preserving at face {}",
            g
        );
    }

    println!("pub const NEIGHBOR_CW: [[u8; 5]; FACES] = [");
    for slot in 0..12 {
        let cells: Vec<String> = table[slot].iter().map(|&v| names[v].to_string()).collect();
        println!("    [{}],", cells.join(", "));
    }
    println!("];");
    println!("pub const Y_PERM: [u8; FACES] = [");
    let cells: Vec<String> = y_perm.iter().map(|&v| names[v].to_string()).collect();
    println!("    [{}],", cells.join(", "));
    println!("];");
    for (slot, face) in ids.iter().enumerate() {
        println!(
            "face {} id {} antipode {} ring {:?}",
            names[slot],
            slot,
            names[id_of(antipode(*face))],
            table[slot]
                .iter()
                .map(|&v| names[v].to_string())
                .collect::<Vec<_>>()
        );
    }
    println!("u ring vertices {:?}", u_ring);
}
