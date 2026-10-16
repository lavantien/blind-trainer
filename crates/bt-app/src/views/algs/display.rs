use crate::theme;
use bevy::prelude::Color;
use bt_puzzles::{FaceletFace, FaceletGrid};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct NetCell {
    pub face: usize,
    pub row: usize,
    pub col: usize,
    pub color: u8,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum NetSlot {
    Cell(NetCell),
    Pad,
}

#[derive(Clone, PartialEq, Debug)]
pub struct NetFace {
    pub face: usize,
    pub width: usize,
    pub rows: Vec<Vec<NetSlot>>,
}

#[derive(Clone, PartialEq, Debug)]
pub enum NetBandSlot {
    Face(NetFace),
    Spacer(usize),
}

#[derive(Clone, PartialEq, Debug, Default)]
pub struct NetLayout {
    pub bands: Vec<Vec<NetBandSlot>>,
}

pub fn net_layout(grid: &FaceletGrid) -> NetLayout {
    let faces: Vec<NetFace> = grid
        .faces
        .iter()
        .enumerate()
        .map(|(face, view)| net_face(face, view))
        .collect();
    let spacer = faces.first().map(|face| face.width).unwrap_or(0);
    NetLayout {
        bands: band_plan(faces.len())
            .into_iter()
            .map(|band| {
                band.into_iter()
                    .map(|slot| match slot {
                        Some(index) => NetBandSlot::Face(faces[index].clone()),
                        None => NetBandSlot::Spacer(spacer),
                    })
                    .collect()
            })
            .collect(),
    }
}

fn net_face(face: usize, view: &FaceletFace) -> NetFace {
    let width = view.rows.iter().map(Vec::len).max().unwrap_or(0);
    let rows = view
        .rows
        .iter()
        .enumerate()
        .map(|(row, cells)| {
            let lead = (width - cells.len()) / 2;
            let trail = width - cells.len() - lead;
            let mut slots = vec![NetSlot::Pad; lead];
            slots.extend(cells.iter().enumerate().map(|(col, &color)| {
                NetSlot::Cell(NetCell {
                    face,
                    row,
                    col,
                    color,
                })
            }));
            slots.extend(vec![NetSlot::Pad; trail]);
            slots
        })
        .collect();
    NetFace { face, width, rows }
}

fn band_plan(face_count: usize) -> Vec<Vec<Option<usize>>> {
    match face_count {
        0 => Vec::new(),
        1 => vec![vec![Some(0)]],
        2 => vec![vec![Some(0)], vec![Some(1)]],
        3 => vec![vec![Some(0), Some(1), Some(2)]],
        4 => vec![vec![Some(0), Some(1)], vec![Some(2), Some(3)]],
        6 => vec![
            vec![None, Some(0)],
            vec![Some(1), Some(2), Some(3), Some(4)],
            vec![None, Some(5)],
        ],
        count => rows_of_four(count),
    }
}

fn rows_of_four(count: usize) -> Vec<Vec<Option<usize>>> {
    let indices: Vec<usize> = (0..count).collect();
    let mut bands: Vec<Vec<Option<usize>>> = indices
        .chunks(4)
        .map(|chunk| chunk.iter().map(|&index| Some(index)).collect())
        .collect();
    if let Some(last) = bands.last_mut() {
        for _ in 0..(4 - last.len()) / 2 {
            last.insert(0, None);
        }
    }
    bands
}

pub fn cell_color(cell: &NetCell) -> Color {
    theme::palette_color(cell.color as usize)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn square_grid(n: usize, color_of: impl Fn(usize, usize, usize) -> u8) -> FaceletGrid {
        FaceletGrid {
            faces: (0..6)
                .map(|face| FaceletFace {
                    rows: (0..n)
                        .map(|row| (0..n).map(|col| color_of(face, row, col)).collect())
                        .collect(),
                })
                .collect(),
        }
    }

    fn ragged_grid(face_count: usize, row_lengths: &[usize]) -> FaceletGrid {
        FaceletGrid {
            faces: (0..face_count)
                .map(|face| FaceletFace {
                    rows: row_lengths
                        .iter()
                        .map(|&len| vec![(face % 12) as u8; len])
                        .collect(),
                })
                .collect(),
        }
    }

    fn collect_cells(layout: &NetLayout) -> Vec<NetCell> {
        let mut cells = Vec::new();
        for band in &layout.bands {
            for slot in band {
                if let NetBandSlot::Face(face) = slot {
                    for row in &face.rows {
                        for slot in row {
                            if let NetSlot::Cell(cell) = slot {
                                cells.push(*cell);
                            }
                        }
                    }
                }
            }
        }
        cells
    }

    fn band_faces(band: &[NetBandSlot]) -> Vec<usize> {
        band.iter()
            .filter_map(|slot| match slot {
                NetBandSlot::Face(face) => Some(face.face),
                NetBandSlot::Spacer(_) => None,
            })
            .collect()
    }

    #[test]
    fn solved_cube_net_follows_the_app_cross() {
        let grid = square_grid(3, |face, _, _| face as u8);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 3);
        assert_eq!(band_faces(&layout.bands[0]), vec![0]);
        assert_eq!(band_faces(&layout.bands[1]), vec![1, 2, 3, 4]);
        assert_eq!(band_faces(&layout.bands[2]), vec![5]);
        assert_eq!(
            layout.bands[0],
            vec![
                NetBandSlot::Spacer(3),
                NetBandSlot::Face(NetFace {
                    face: 0,
                    width: 3,
                    rows: vec![
                        vec![
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 0,
                                col: 0,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 0,
                                col: 1,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 0,
                                col: 2,
                                color: 0
                            }),
                        ],
                        vec![
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 1,
                                col: 0,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 1,
                                col: 1,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 1,
                                col: 2,
                                color: 0
                            }),
                        ],
                        vec![
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 2,
                                col: 0,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 2,
                                col: 1,
                                color: 0
                            }),
                            NetSlot::Cell(NetCell {
                                face: 0,
                                row: 2,
                                col: 2,
                                color: 0
                            }),
                        ],
                    ],
                }),
            ]
        );
        assert_eq!(layout.bands[2].len(), 2);
        assert_eq!(layout.bands[2][0], NetBandSlot::Spacer(3));
        let NetBandSlot::Face(face) = &layout.bands[2][1] else {
            panic!("bottom slot must be a face");
        };
        assert_eq!(face.face, 5);
        assert_eq!(face.width, 3);
        assert_eq!(face.rows.len(), 3);
        for row in &face.rows {
            assert_eq!(row.len(), 3);
            for slot in row {
                let NetSlot::Cell(cell) = slot else {
                    panic!("no pads expected in square faces");
                };
                assert_eq!(cell.color, 5);
            }
        }
    }

    #[test]
    fn every_facelet_maps_exactly_once() {
        let grid = square_grid(3, |face, row, col| ((face * 9 + row * 3 + col) % 12) as u8);
        let layout = net_layout(&grid);
        let cells = collect_cells(&layout);
        assert_eq!(cells.len(), 54);
        let mut seen = std::collections::BTreeSet::new();
        for cell in cells {
            assert_eq!(
                cell.color,
                ((cell.face * 9 + cell.row * 3 + cell.col) % 12) as u8
            );
            assert!(seen.insert((cell.face, cell.row, cell.col)), "dup {cell:?}");
        }
        assert_eq!(seen.len(), 54);
        for band in &layout.bands {
            for slot in band {
                if let NetBandSlot::Face(face) = slot {
                    for row in &face.rows {
                        assert_eq!(row.len(), 3);
                        assert!(row.iter().all(|slot| matches!(slot, NetSlot::Cell(_))));
                    }
                }
            }
        }
    }

    #[test]
    fn seven_by_seven_keeps_face_and_stride() {
        let grid = square_grid(7, |face, row, col| ((face + row + col) % 12) as u8);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 3);
        assert_eq!(band_faces(&layout.bands[0]), vec![0]);
        assert_eq!(band_faces(&layout.bands[1]), vec![1, 2, 3, 4]);
        assert_eq!(band_faces(&layout.bands[2]), vec![5]);
        assert_eq!(layout.bands[0][0], NetBandSlot::Spacer(7));
        assert_eq!(layout.bands[2][0], NetBandSlot::Spacer(7));
        let cells = collect_cells(&layout);
        assert_eq!(cells.len(), 6 * 49);
        for band in &layout.bands {
            for slot in band {
                if let NetBandSlot::Face(face) = slot {
                    assert_eq!(face.width, 7);
                    assert_eq!(face.rows.len(), 7);
                    for (row_index, row) in face.rows.iter().enumerate() {
                        assert_eq!(row.len(), 7);
                        for (col_index, slot) in row.iter().enumerate() {
                            let NetSlot::Cell(cell) = slot else {
                                panic!("no pads expected in square faces");
                            };
                            assert_eq!(
                                cell.color,
                                ((cell.face + row_index + col_index) % 12) as u8
                            );
                        }
                    }
                }
            }
        }
    }

    #[test]
    fn triangle_faces_center_ragged_rows() {
        let grid = ragged_grid(4, &[1, 3, 5]);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 2);
        assert_eq!(band_faces(&layout.bands[0]), vec![0, 1]);
        assert_eq!(band_faces(&layout.bands[1]), vec![2, 3]);
        let NetBandSlot::Face(face) = &layout.bands[0][0] else {
            panic!("first slot must be a face");
        };
        assert_eq!(face.width, 5);
        assert_eq!(face.rows.len(), 3);
        assert_eq!(
            face.rows[0],
            vec![
                NetSlot::Pad,
                NetSlot::Pad,
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 0,
                    col: 0,
                    color: 0
                }),
                NetSlot::Pad,
                NetSlot::Pad,
            ]
        );
        assert_eq!(
            face.rows[1],
            vec![
                NetSlot::Pad,
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 1,
                    col: 0,
                    color: 0
                }),
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 1,
                    col: 1,
                    color: 0
                }),
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 1,
                    col: 2,
                    color: 0
                }),
                NetSlot::Pad,
            ]
        );
        assert!(
            face.rows[2]
                .iter()
                .all(|slot| matches!(slot, NetSlot::Cell(_)))
        );
        assert_eq!(collect_cells(&layout).len(), 4 * 9);
    }

    #[test]
    fn pentagon_faces_center_ragged_rows() {
        let grid = ragged_grid(12, &[1, 2, 3, 3, 2]);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 3);
        assert_eq!(band_faces(&layout.bands[0]), vec![0, 1, 2, 3]);
        assert_eq!(band_faces(&layout.bands[1]), vec![4, 5, 6, 7]);
        assert_eq!(band_faces(&layout.bands[2]), vec![8, 9, 10, 11]);
        let NetBandSlot::Face(face) = &layout.bands[0][0] else {
            panic!("first slot must be a face");
        };
        assert_eq!(face.width, 3);
        assert_eq!(face.rows.len(), 5);
        assert_eq!(
            face.rows[0],
            vec![
                NetSlot::Pad,
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 0,
                    col: 0,
                    color: 0
                }),
                NetSlot::Pad,
            ]
        );
        assert_eq!(
            face.rows[1],
            vec![
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 1,
                    col: 0,
                    color: 0
                }),
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 1,
                    col: 1,
                    color: 0
                }),
                NetSlot::Pad,
            ]
        );
        assert_eq!(
            face.rows[4],
            vec![
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 4,
                    col: 0,
                    color: 0
                }),
                NetSlot::Cell(NetCell {
                    face: 0,
                    row: 4,
                    col: 1,
                    color: 0
                }),
                NetSlot::Pad,
            ]
        );
        assert_eq!(collect_cells(&layout).len(), 12 * 11);
    }

    #[test]
    fn two_layers_stack_vertically() {
        let grid = ragged_grid(2, &[12]);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 2);
        assert_eq!(band_faces(&layout.bands[0]), vec![0]);
        assert_eq!(band_faces(&layout.bands[1]), vec![1]);
        for band in &layout.bands {
            assert_eq!(band.len(), 1);
            let NetBandSlot::Face(face) = &band[0] else {
                panic!("single face band");
            };
            assert_eq!(face.width, 12);
            assert_eq!(collect_cells_of_face(face), 12);
        }
        assert_eq!(collect_cells(&layout).len(), 24);
    }

    fn collect_cells_of_face(face: &NetFace) -> usize {
        face.rows
            .iter()
            .flat_map(|row| row.iter())
            .filter(|slot| matches!(slot, NetSlot::Cell(_)))
            .count()
    }

    #[test]
    fn short_odd_band_centers_with_a_leading_spacer() {
        let grid = ragged_grid(5, &[3, 3, 3]);
        let layout = net_layout(&grid);
        assert_eq!(layout.bands.len(), 2);
        assert_eq!(band_faces(&layout.bands[0]), vec![0, 1, 2, 3]);
        assert_eq!(layout.bands[1].len(), 2);
        assert_eq!(layout.bands[1][0], NetBandSlot::Spacer(3));
        let NetBandSlot::Face(face) = &layout.bands[1][1] else {
            panic!("centered slot must be a face");
        };
        assert_eq!(face.face, 4);
        assert_eq!(collect_cells_of_face(face), 9);
    }

    #[test]
    fn empty_grid_renders_nothing() {
        let layout = net_layout(&FaceletGrid { faces: vec![] });
        assert_eq!(layout, NetLayout::default());
    }

    #[test]
    fn cell_color_reads_the_palette() {
        let cell = NetCell {
            face: 2,
            row: 0,
            col: 0,
            color: 7,
        };
        assert_eq!(cell_color(&cell), theme::palette_color(7));
        assert_ne!(cell_color(&cell), theme::palette_color(6));
        let wrapped = NetCell { color: 12, ..cell };
        assert_eq!(cell_color(&wrapped), theme::palette_color(0));
    }
}
