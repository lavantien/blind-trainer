use bevy::prelude::*;

pub const ROOT_REM: f32 = 16.0;
pub const FONT_TITLE: f32 = 1.75;
pub const FONT_HEADING: f32 = 1.25;
pub const FONT_BODY: f32 = 1.0;
pub const FONT_SMALL: f32 = 0.85;
pub const SPACING_REM: f32 = 0.5;
pub const PANEL_PADDING_REM: f32 = 1.0;
pub const PULSE_HZ: f32 = 2.0;
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

pub fn buffer_highlight() -> Color {
    Color::srgb(0.30, 0.90, 0.85)
}

pub fn target_highlight() -> Color {
    Color::srgb(0.98, 0.55, 0.25)
}

pub const FACE_COUNT: usize = 6;

pub fn face_color(face: usize) -> Color {
    let shades: [(f32, f32, f32); FACE_COUNT] = [
        (0.95, 0.95, 0.95),
        (0.95, 0.60, 0.15),
        (0.20, 0.75, 0.35),
        (0.85, 0.20, 0.20),
        (0.20, 0.45, 0.90),
        (0.95, 0.85, 0.15),
    ];
    let (r, g, b) = shades[face % FACE_COUNT];
    Color::srgb(r, g, b)
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
    fn font_sizes_form_a_scale() {
        let title = FONT_TITLE;
        let heading = FONT_HEADING;
        let body = FONT_BODY;
        let small = FONT_SMALL;
        assert!(title > heading && heading > body && body > small && small > 0.0);
    }
}
