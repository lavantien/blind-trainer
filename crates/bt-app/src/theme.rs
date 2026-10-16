use bevy::prelude::*;

pub const ROOT_REM: f32 = 20.0;
pub const FONT_FOCAL: f32 = 3.4;
pub const FONT_TITLE: f32 = 2.2;
pub const FONT_ALG: f32 = 1.7;
pub const FONT_HEADING: f32 = 1.3;
pub const FONT_BODY: f32 = 1.0;
pub const FONT_SMALL: f32 = 0.8;
pub const FONT_MICRO: f32 = 0.75;
pub const SPACING_REM: f32 = 0.5;
pub const PANEL_PADDING_REM: f32 = 1.0;
pub const HEADER_HEIGHT_REM: f32 = 3.5;
pub const HAIRLINE_REM: f32 = 0.05;
pub const DRILL_NET_WIDTH_PCT: f32 = 0.44;
pub const PULSE_HZ: f32 = 2.0;
pub const NET_VIEW_RESERVE_REM: f32 = 8.0;
pub const DYNAMIC_BAND_REM: f32 = 16.5;
pub const PULSE_LERP_PER_S: f32 = 8.0;

pub fn background() -> Color {
    Color::srgb(0.09, 0.10, 0.12)
}

pub fn panel() -> Color {
    Color::srgb(0.14, 0.15, 0.18)
}

pub fn panel_raised() -> Color {
    Color::srgb(0.19, 0.20, 0.24)
}

pub fn text_primary() -> Color {
    Color::srgb(0.92, 0.93, 0.95)
}

pub fn text_muted() -> Color {
    Color::srgb(0.62, 0.64, 0.68)
}

pub fn accent() -> Color {
    Color::srgb(0.35, 0.66, 0.95)
}

pub fn danger() -> Color {
    Color::srgb(0.92, 0.35, 0.35)
}

pub fn teal() -> Color {
    Color::srgb(0.30, 0.90, 0.85)
}

pub fn interchange() -> Color {
    Color::srgb(0.98, 0.55, 0.25)
}

pub fn setup() -> Color {
    Color::srgb(0.55, 0.62, 0.75)
}

pub fn ok() -> Color {
    Color::srgb(0.36, 0.78, 0.44)
}

pub fn hairline() -> Color {
    Color::srgb(0.26, 0.28, 0.33)
}

pub const FACE_COUNT: usize = 6;
pub const PALETTE_COUNT: usize = 12;

const FACE_SHADES: [(f32, f32, f32); PALETTE_COUNT] = [
    (0.95, 0.95, 0.95),
    (0.95, 0.60, 0.15),
    (0.20, 0.75, 0.35),
    (0.85, 0.20, 0.20),
    (0.20, 0.45, 0.90),
    (0.95, 0.85, 0.15),
    (0.15, 0.85, 0.90),
    (0.90, 0.35, 0.80),
    (0.55, 0.30, 0.85),
    (0.60, 0.40, 0.20),
    (0.55, 0.58, 0.62),
    (0.10, 0.55, 0.55),
];

pub fn palette_color(index: usize) -> Color {
    let (r, g, b) = FACE_SHADES[index % PALETTE_COUNT];
    Color::srgb(r, g, b)
}

pub fn face_color(face: usize) -> Color {
    palette_color(face)
}

pub fn face_name(face: usize) -> &'static str {
    ["U", "L", "F", "R", "B", "D"][face % FACE_COUNT]
}

pub fn rem(value: f32) -> Val {
    Val::Rem(value)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn face_colors_follow_cube_convention() {
        assert_eq!(face_name(0), "U");
        assert_eq!(face_name(1), "L");
        assert_eq!(face_name(2), "F");
        assert_eq!(face_name(3), "R");
        assert_eq!(face_name(4), "B");
        assert_eq!(face_name(5), "D");
    }

    #[test]
    fn font_scale_is_pinned() {
        assert_eq!(FONT_MICRO, 0.75);
        assert_eq!(FONT_SMALL, 0.8);
        assert_eq!(FONT_BODY, 1.0);
        assert_eq!(FONT_HEADING, 1.3);
        assert_eq!(FONT_ALG, 1.7);
        assert_eq!(FONT_TITLE, 2.2);
        assert_eq!(FONT_FOCAL, 3.4);
    }

    #[test]
    fn mechanic_palette_is_distinct() {
        let colors = [teal(), interchange(), setup(), ok()];
        for (i, left) in colors.iter().enumerate() {
            for right in colors.iter().skip(i + 1) {
                assert_ne!(left.to_linear(), right.to_linear());
            }
        }
    }

    #[test]
    fn palette_pins_twelve_shades() {
        let pinned: [(f32, f32, f32); PALETTE_COUNT] = [
            (0.95, 0.95, 0.95),
            (0.95, 0.60, 0.15),
            (0.20, 0.75, 0.35),
            (0.85, 0.20, 0.20),
            (0.20, 0.45, 0.90),
            (0.95, 0.85, 0.15),
            (0.15, 0.85, 0.90),
            (0.90, 0.35, 0.80),
            (0.55, 0.30, 0.85),
            (0.60, 0.40, 0.20),
            (0.55, 0.58, 0.62),
            (0.10, 0.55, 0.55),
        ];
        for (index, shade) in pinned.iter().enumerate() {
            assert_eq!(
                palette_color(index),
                Color::srgb(shade.0, shade.1, shade.2),
                "palette slot {index}"
            );
        }
        assert_eq!(palette_color(PALETTE_COUNT), palette_color(0));
    }

    #[test]
    fn palette_is_pairwise_distinct() {
        for left in 0..PALETTE_COUNT {
            for right in (left + 1)..PALETTE_COUNT {
                let a = palette_color(left).to_linear();
                let b = palette_color(right).to_linear();
                let distance = ((a.red - b.red).powi(2)
                    + (a.green - b.green).powi(2)
                    + (a.blue - b.blue).powi(2))
                .sqrt();
                assert!(
                    distance >= 0.2,
                    "palette {left} vs {right} distance {distance}"
                );
            }
        }
    }

    #[test]
    fn palette_front_six_match_the_cube_faces() {
        for face in 0..FACE_COUNT {
            assert_eq!(palette_color(face), face_color(face), "face {face}");
        }
    }
}
