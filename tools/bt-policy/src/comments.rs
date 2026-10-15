use crate::Finding;
use crate::lexer::{line_of, mask_source};

pub fn findings(text: &str, path: &str) -> Vec<Finding> {
    mask_source(text)
        .comments
        .iter()
        .map(|span| Finding {
            path: path.to_string(),
            line: line_of(text, span.start),
            kind: "comment",
            detail: format!("{} comment", span.kind.as_str()),
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_comment_becomes_a_finding_with_line_and_kind() {
        let input = "let a = 1;\n// line two\n/* block\nspanning */ let b = 2;";
        let found = findings(input, "src/dirty.rs");
        assert_eq!(found.len(), 2);
        assert_eq!(
            found[0],
            Finding {
                path: "src/dirty.rs".to_string(),
                line: 2,
                kind: "comment",
                detail: "line comment".to_string(),
            }
        );
        assert_eq!(found[1].line, 3);
        assert_eq!(found[1].detail, "block comment");
    }

    #[test]
    fn comment_free_source_yields_no_findings() {
        let input = "let s = \"http://example.org/a//b\";\nlet t = 1;";
        assert!(findings(input, "clean.rs").is_empty());
    }
}
