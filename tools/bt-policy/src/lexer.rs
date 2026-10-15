#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum CommentKind {
    Line,
    Block,
}

impl CommentKind {
    pub fn as_str(self) -> &'static str {
        match self {
            CommentKind::Line => "line",
            CommentKind::Block => "block",
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct CommentSpan {
    pub start: usize,
    pub end: usize,
    pub kind: CommentKind,
}

pub struct Masked {
    pub text: String,
    pub comments: Vec<CommentSpan>,
}

pub fn line_of(text: &str, offset: usize) -> usize {
    text.as_bytes()[..offset]
        .iter()
        .filter(|byte| **byte == b'\n')
        .count()
        + 1
}

pub fn mask_source(text: &str) -> Masked {
    let bytes = text.as_bytes();
    let len = bytes.len();
    let mut out = bytes.to_vec();
    let mut comments = Vec::new();
    let mut i = 0;
    while i < len {
        let prev_word = i > 0 && is_word(bytes[i - 1]);
        match bytes[i] {
            b'/' if bytes.get(i + 1) == Some(&b'/') => {
                let end = bytes[i + 2..]
                    .iter()
                    .position(|byte| *byte == b'\n')
                    .map_or(len, |at| i + 2 + at);
                comments.push(CommentSpan {
                    start: i,
                    end,
                    kind: CommentKind::Line,
                });
                out[i..end].fill(b' ');
                i = end;
            }
            b'/' if bytes.get(i + 1) == Some(&b'*') => {
                let end = block_end(bytes, i);
                comments.push(CommentSpan {
                    start: i,
                    end,
                    kind: CommentKind::Block,
                });
                out[i..end].fill(b' ');
                i = end;
            }
            b'"' => {
                if let Some(close) = string_close(bytes, i) {
                    out[i + 1..close].fill(b' ');
                    i = close + 1;
                } else {
                    out[i + 1..].fill(b' ');
                    i = len;
                }
            }
            b'\'' => {
                if let Some(end) = char_close(bytes, i) {
                    out[i + 1..end - 1].fill(b' ');
                    i = end;
                } else {
                    i += 1;
                }
            }
            b'r' if !prev_word => match raw_string_close(bytes, i + 1) {
                Some((content, close, end)) => {
                    out[content..close].fill(b' ');
                    i = end;
                }
                None => i += 1,
            },
            b'b' if !prev_word => match bytes.get(i + 1) {
                Some(b'\'') => {
                    if let Some(end) = char_close(bytes, i + 1) {
                        out[i + 2..end - 1].fill(b' ');
                        i = end;
                    } else {
                        i += 1;
                    }
                }
                Some(b'"') => {
                    if let Some(close) = string_close(bytes, i + 1) {
                        out[i + 2..close].fill(b' ');
                        i = close + 1;
                    } else {
                        i += 1;
                    }
                }
                Some(b'r') => match raw_string_close(bytes, i + 2) {
                    Some((content, close, end)) => {
                        out[content..close].fill(b' ');
                        i = end;
                    }
                    None => i += 1,
                },
                _ => i += 1,
            },
            _ => i += 1,
        }
    }
    Masked {
        text: String::from_utf8(out).expect("masking replaces bytes with spaces"),
        comments,
    }
}

fn is_word(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn char_len(byte: u8) -> usize {
    if byte < 0x80 {
        1
    } else if byte >> 5 == 0b110 {
        2
    } else if byte >> 4 == 0b1110 {
        3
    } else {
        4
    }
}

fn block_end(bytes: &[u8], start: usize) -> usize {
    let mut j = start + 2;
    let mut depth = 1u32;
    while j < bytes.len() {
        if bytes[j] == b'/' && bytes.get(j + 1) == Some(&b'*') {
            depth += 1;
            j += 2;
        } else if bytes[j] == b'*' && bytes.get(j + 1) == Some(&b'/') {
            depth -= 1;
            j += 2;
            if depth == 0 {
                return j;
            }
        } else {
            j += 1;
        }
    }
    bytes.len()
}

fn string_close(bytes: &[u8], start: usize) -> Option<usize> {
    let mut j = start + 1;
    while j < bytes.len() {
        match bytes[j] {
            b'\\' => j += 2,
            b'"' => return Some(j),
            _ => j += 1,
        }
    }
    None
}

fn raw_string_close(bytes: &[u8], mut probe: usize) -> Option<(usize, usize, usize)> {
    let hashes_start = probe;
    while bytes.get(probe) == Some(&b'#') {
        probe += 1;
    }
    let hashes = probe - hashes_start;
    if bytes.get(probe) != Some(&b'"') {
        return None;
    }
    let content = probe + 1;
    let mut q = content;
    while q < bytes.len() {
        if bytes[q] == b'"' {
            let mut r = q + 1;
            let mut matched = 0;
            while matched < hashes && bytes.get(r) == Some(&b'#') {
                r += 1;
                matched += 1;
            }
            if matched == hashes {
                return Some((content, q, r));
            }
        }
        q += 1;
    }
    None
}

fn char_close(bytes: &[u8], start: usize) -> Option<usize> {
    let quote = start + 1;
    match bytes.get(quote) {
        None | Some(b'\'') => None,
        Some(b'\\') => {
            let mut j = quote + 1;
            match bytes.get(j) {
                None => None,
                Some(b'u') if bytes.get(j + 1) == Some(&b'{') => {
                    j += 2;
                    while bytes.get(j).is_some_and(|byte| *byte != b'}') {
                        j += 1;
                    }
                    if bytes.get(j) == Some(&b'}') {
                        j += 1;
                    }
                    close_after(bytes, j)
                }
                Some(b'x') => {
                    j += 3;
                    close_after(bytes, j)
                }
                _ => close_after(bytes, j + 1),
            }
        }
        Some(_) => {
            let width = char_len(bytes[quote]);
            close_after(bytes, quote + width)
        }
    }
}

fn close_after(bytes: &[u8], probe: usize) -> Option<usize> {
    match bytes.get(probe) {
        Some(b'\'') => Some(probe + 1),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn line_comment_is_masked_and_reported_with_exact_span() {
        let input = "let x = 1; // note\nlet y = 2;";
        let masked = mask_source(input);
        assert_eq!(
            masked.text,
            format!("let x = 1; {}\nlet y = 2;", " ".repeat(7))
        );
        assert_eq!(masked.comments.len(), 1);
        let span = masked.comments[0];
        assert_eq!(span.kind, CommentKind::Line);
        assert_eq!(&input[span.start..span.end], "// note");
        assert_eq!(line_of(input, span.start), 1);
    }

    #[test]
    fn line_comment_at_eof_covers_the_tail() {
        let input = "let x = 1; // tail";
        let masked = mask_source(input);
        let span = masked.comments[0];
        assert_eq!(&input[span.start..span.end], "// tail");
        assert_eq!(span.end, input.len());
    }

    #[test]
    fn doc_comment_markers_are_line_comments() {
        let input = "/// doc\n//! inner\nlet x = 1;";
        let masked = mask_source(input);
        assert_eq!(masked.comments.len(), 2);
        assert_eq!(
            &input[masked.comments[0].start..masked.comments[0].end],
            "/// doc"
        );
        assert_eq!(line_of(input, masked.comments[0].start), 1);
        assert_eq!(
            &input[masked.comments[1].start..masked.comments[1].end],
            "//! inner"
        );
        assert_eq!(line_of(input, masked.comments[1].start), 2);
    }

    #[test]
    fn nested_block_comment_is_one_span() {
        let input = "let a = 1; /* outer /* inner */ still */ let b = 2;";
        let masked = mask_source(input);
        assert_eq!(masked.comments.len(), 1);
        let span = masked.comments[0];
        assert_eq!(span.kind, CommentKind::Block);
        assert_eq!(
            &input[span.start..span.end],
            "/* outer /* inner */ still */"
        );
        assert_eq!(
            masked.text,
            format!(
                "let a = 1; {} let b = 2;",
                " ".repeat(span.end - span.start)
            )
        );
    }

    #[test]
    fn urls_inside_strings_are_not_comments() {
        let input = r#"let s = "http://example.org/x"; let t = 2;"#;
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(
            masked.text,
            format!("let s = \"{}\"; let t = 2;", " ".repeat(20))
        );
    }

    #[test]
    fn escaped_quote_keeps_the_string_open() {
        let input = r#"let s = "a\"b//c"; let t = 1;"#;
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(
            masked.text,
            format!("let s = \"{}\"; let t = 1;", " ".repeat(7))
        );
    }

    #[test]
    fn raw_string_with_hashes_spans_newlines() {
        let input = "let r = r##\"line1\nwith // and /* inside\"##; let t = 3;";
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        let head = "let r = r##\"";
        let tail = "\"##; let t = 3;";
        assert_eq!(masked.text.len(), input.len());
        assert!(masked.text.starts_with(head));
        assert!(masked.text.ends_with(tail));
        let middle = &masked.text[head.len()..masked.text.len() - tail.len()];
        assert!(
            middle.chars().all(|ch| ch == ' '),
            "middle must be spaces, got {middle:?}"
        );
    }

    #[test]
    fn plain_raw_string_closes_at_the_next_quote() {
        let input = r#"let r = r"a // b"; let t = 8;"#;
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(masked.text, "let r = r\"      \"; let t = 8;");
    }

    #[test]
    fn byte_string_contents_are_masked() {
        let input = r#"let b = b"a//b"; let t = 4;"#;
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(masked.text, "let b = b\"    \"; let t = 4;");
    }

    #[test]
    fn raw_byte_string_contents_are_masked() {
        let input = r##"let rb = br#"x/*y"#; let t = 5;"##;
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(masked.text, "let rb = br#\"    \"#; let t = 5;");
    }

    #[test]
    fn char_literals_of_every_shape_are_masked() {
        let input = r"let a = 'x'; let b = '\n'; let c = '\''; let d = '\u{1F600}';";
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(
            masked.text,
            "let a = ' '; let b = '  '; let c = '  '; let d = '         ';"
        );
    }

    #[test]
    fn byte_char_is_masked_with_its_prefix_kept() {
        let input = r"let bc = b'z'; let t = 6;";
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(masked.text, "let bc = b' '; let t = 6;");
    }

    #[test]
    fn lifetimes_and_labels_stay_code() {
        let input = "fn f<'a>(x: &'a str) -> &'static str { 'outer: loop { break 'outer; } }";
        let masked = mask_source(input);
        assert!(masked.comments.is_empty());
        assert_eq!(masked.text, input);
    }

    #[test]
    fn masking_preserves_byte_length_with_multibyte_content() {
        let input = "let s = \"héllo // wörld\"; let t = 7;";
        let masked = mask_source(input);
        assert_eq!(masked.text.len(), input.len());
        let head = "let s = \"";
        let tail = "\"; let t = 7;";
        let content_len = input.len() - head.len() - tail.len();
        assert_eq!(
            masked.text,
            format!("let s = \"{}\"; let t = 7;", " ".repeat(content_len))
        );
    }

    #[test]
    fn line_of_counts_newlines_before_the_offset() {
        let text = "ab\ncd\nef";
        assert_eq!(line_of(text, 0), 1);
        assert_eq!(line_of(text, 3), 2);
        assert_eq!(line_of(text, 6), 3);
    }
}
