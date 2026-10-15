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

fn main() {
    let cases: Vec<(&str, &str)> = vec![
        ("escape eof", "\"abc\\"),
        ("unterm raw plain", r#"r"abc // x"#),
        ("unterm raw hashes", r##"r#"abc /* x"##),
        ("unterm str", "let s = \"abc"),
        ("lone quote", "let a = '"),
        ("empty quotes", "'''"),
        ("trunc uchar", r"'\u{'"),
        ("trunc xchar", r"'\x4'"),
        ("shebang", "#!/usr/bin/env rust\nfn f() {}\n"),
        ("block minimal", "/**/fn f(){}"),
        ("block slash star slash", "/*/fn f(){}"),
        ("unbalanced inner", "/* /* */ fn f(){}"),
        ("div ops", "let a = b / c; let d = e /f; let g = h/ i; let j = k /= l;"),
        ("byte char slash", r#"let b = b'/'; let c = '/'; let d = '\\';"#),
        ("b r char", r#"let a = b'r'; let e = b'\\'; let f = br'x';"#),
        ("raw ident", r##"let r#enum = r#"str // x"#; fn g(r#type: u8) {}"##),
        ("str cont", "\"a\\\nb // c\"; let x = 1;"),
        ("nul char", r"'\0'"),
        ("multibyte char", "'é' + '字'"),
        ("raw hashes inner", "r##\"a\"# b\"##; let y = 2;"),
        ("crlf comment", "let a = 1; // x\r\nlet b = 2;\r\n"),
        ("quote last byte", "let a = \""),
        ("u escape quote inside", r"'\u{7}'"),
        ("raw zero hashes empty", "r\"\"; let a = 1;"),
        ("label like char", "fn f() { 'outer: loop { break 'outer; } }"),
        ("comment then string", "/* \" */ let s = \"*/ nope\"; // tail"),
    ];
    for (name, input) in cases {
        let before = input.len();
        let masked = mask_source(input);
        assert_eq!(masked.text.len(), before, "length changed: {name}");
        let spans: Vec<String> = masked
            .comments
            .iter()
            .map(|s| {
                format!(
                    "{}..{} {} {:?}",
                    s.start,
                    s.end,
                    s.kind.as_str(),
                    &input[s.start..s.end]
                )
            })
            .collect();
        println!(
            "{name}: len ok, comments={}",
            if spans.is_empty() {
                "none".to_string()
            } else {
                spans.join(" | ")
            }
        );
        println!("  masked: {:?}", masked.text);
    }
}
