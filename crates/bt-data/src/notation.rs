pub fn is_rotation(token: &str) -> bool {
    let mut chars = token.chars();
    matches!(chars.next(), Some('x' | 'y' | 'z')) && chars.all(|ch| ch == '\'' || ch == '2')
}

pub fn strip_rotations(alg: &str) -> String {
    alg.split_whitespace()
        .filter(|token| !is_rotation(token))
        .collect::<Vec<&str>>()
        .join(" ")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rotation_tokens_cover_all_three_axes_and_suffixes() {
        for token in ["x", "y", "z", "x2", "y'", "z2'", "x'2"] {
            assert!(is_rotation(token), "{token}");
        }
        for token in ["F", "R'", "L2", "u", "w", "xx", ""] {
            assert!(!is_rotation(token), "{token}");
        }
    }

    #[test]
    fn strip_rotations_removes_regrip_labels_only() {
        assert_eq!(strip_rotations("y2 R' F R F'"), "R' F R F'");
        assert_eq!(
            strip_rotations("y2 F' L F L' F' L F L'"),
            "F' L F L' F' L F L'"
        );
        assert_eq!(
            strip_rotations("R' F R F' y2 R' F R F' z2 x' R' F R F'"),
            "R' F R F' R' F R F' R' F R F'"
        );
        assert_eq!(strip_rotations("L F' L'"), "L F' L'");
        assert_eq!(strip_rotations("y2 y'"), "");
    }
}
