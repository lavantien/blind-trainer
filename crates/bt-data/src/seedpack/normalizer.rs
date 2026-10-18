use std::fmt;

pub const SQ1_EVENT: &str = "sq1";
const REPEAT_LIMIT: usize = 99;
const EXPAND_LIMIT: usize = 100_000;
const TURN_MIN: i64 = -5;
const HALF_RING: i64 = 6;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Normalized {
    pub alg: String,
    pub notes: Vec<String>,
}

#[derive(Debug)]
pub enum NormalizeError {
    UnknownToken { alg: String, token: String },
    Unbalanced { alg: String },
    BadRepeat { alg: String },
    BadChain { alg: String },
    EmptyAlg,
}

impl fmt::Display for NormalizeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NormalizeError::UnknownToken { alg, token } => {
                write!(f, "unknown token '{token}' in alg '{alg}'")
            }
            NormalizeError::Unbalanced { alg } => write!(f, "unbalanced parens in alg '{alg}'"),
            NormalizeError::BadRepeat { alg } => {
                write!(f, "malformed repeat marker in alg '{alg}'")
            }
            NormalizeError::BadChain { alg } => {
                write!(f, "chain marker with an empty alg in '{alg}'")
            }
            NormalizeError::EmptyAlg => write!(f, "alg is empty"),
        }
    }
}

impl std::error::Error for NormalizeError {}

pub fn normalize_alg(event: &str, raw: &str) -> Result<Normalized, NormalizeError> {
    if event == SQ1_EVENT {
        normalize_sq1(raw)
    } else {
        normalize_cube(raw)
    }
}

fn normalize_cube(raw: &str) -> Result<Normalized, NormalizeError> {
    let trimmed = raw.trim();
    let mut notes = Vec::new();
    let mut text = trimmed.to_string();
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if let Some((left, right)) = text.split_once("->") {
        let alg_part = left.trim();
        let note_part = right.trim();
        if alg_part.is_empty() || note_part.is_empty() {
            return Err(NormalizeError::BadChain {
                alg: trimmed.into(),
            });
        }
        notes.push(format!("chain note: -> {note_part}"));
        text = alg_part.to_string();
    }
    text = expand_bracket_repeats(&text)?;
    text = expand_group_repeats(&text)?;
    text = strip_grouping_parens(&text)?;
    text = text.replace(',', " ");
    let tokens: Vec<&str> = text.split_whitespace().collect();
    if tokens.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if tokens.iter().all(|token| is_prose_word(token)) {
        notes.push(format!("prose alg: {trimmed}"));
        return Ok(Normalized {
            alg: String::new(),
            notes,
        });
    }
    let mut out = Vec::with_capacity(tokens.len());
    for token in tokens {
        if token == "X" {
            notes.push("mapped X to x".to_string());
            out.push("x".to_string());
        } else if valid_cube_token(token) {
            out.push(token.to_string());
        } else {
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: token.to_string(),
            });
        }
    }
    Ok(Normalized {
        alg: out.join(" "),
        notes,
    })
}

fn is_prose_word(token: &str) -> bool {
    token.len() >= 2 && token.bytes().all(|byte| byte.is_ascii_lowercase())
}

fn valid_cube_token(token: &str) -> bool {
    let bytes = token.as_bytes();
    let mut index = 0;
    if bytes.len() > 1 && bytes[0].is_ascii_digit() {
        if !(b'2'..=b'7').contains(&bytes[0]) {
            return false;
        }
        index = 1;
    }
    if index >= bytes.len() || !bytes[index].is_ascii_alphabetic() {
        return false;
    }
    if bytes[index] == b'w' || bytes[index] == b'W' {
        return false;
    }
    index += 1;
    if index < bytes.len()
        && (bytes[index] == b'w'
            || (bytes[index].is_ascii_uppercase() && bytes[index - 1].is_ascii_uppercase()))
    {
        index += 1;
    }
    let (mut prime, mut two) = (false, false);
    while index < bytes.len() {
        match bytes[index] {
            b'\'' if !prime => prime = true,
            b'2' if !two => two = true,
            _ => return false,
        }
        index += 1;
    }
    true
}

fn expand_bracket_repeats(text: &str) -> Result<String, NormalizeError> {
    let mut current = text.to_string();
    while let Some(open) = current.find('[') {
        let Some(rel) = current[open + 1..].find(']') else {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        };
        let close = open + 1 + rel;
        let content = &current[open + 1..close];
        if content.contains('[') || content.contains(']') {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let tail = current[close + 1..].trim_start();
        let Some(star_tail) = tail.strip_prefix('*') else {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        };
        let digit_len = star_tail
            .bytes()
            .take_while(|byte| byte.is_ascii_digit())
            .count();
        if digit_len == 0 {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let count: usize = star_tail[..digit_len].parse().unwrap_or(0);
        if count == 0 || count > REPEAT_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let after = star_tail[digit_len..].trim_start();
        if !after.is_empty() {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let repeated = vec![content.trim(); count].join(" ");
        if repeated.len() > EXPAND_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        current = format!("{} {} {}", &current[..open], repeated, after)
            .trim()
            .to_string();
    }
    Ok(current)
}

struct RepeatHit {
    open: usize,
    close: usize,
    count: usize,
    digits: usize,
}

fn find_group_repeat(text: &str) -> Result<Option<RepeatHit>, NormalizeError> {
    let bytes = text.as_bytes();
    let mut stack: Vec<usize> = Vec::new();
    let mut index = 0;
    while index < bytes.len() {
        match bytes[index] {
            b'(' => stack.push(index),
            b')' => {
                let Some(open) = stack.pop() else {
                    return Err(NormalizeError::Unbalanced { alg: text.into() });
                };
                let inner = &text[open + 1..index];
                if !inner.contains('(') && !inner.contains(')') {
                    let rest = &bytes[index + 1..];
                    let digits = rest.iter().take_while(|byte| byte.is_ascii_digit()).count();
                    if digits > 0 {
                        let count: usize = text[index + 1..index + 1 + digits].parse().unwrap_or(0);
                        let after = rest.get(digits);
                        if count == 0
                            || count > REPEAT_LIMIT
                            || after
                                .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'\'')
                        {
                            return Err(NormalizeError::BadRepeat { alg: text.into() });
                        }
                        return Ok(Some(RepeatHit {
                            open,
                            close: index,
                            count,
                            digits,
                        }));
                    }
                    if rest
                        .first()
                        .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'\'')
                    {
                        return Err(NormalizeError::BadRepeat { alg: text.into() });
                    }
                }
            }
            _ => {}
        }
        index += 1;
    }
    if !stack.is_empty() {
        return Err(NormalizeError::Unbalanced { alg: text.into() });
    }
    Ok(None)
}

fn expand_group_repeats(text: &str) -> Result<String, NormalizeError> {
    let mut current = text.to_string();
    while let Some(hit) = find_group_repeat(&current)? {
        let content = current[hit.open + 1..hit.close].trim();
        let repeated = vec![content; hit.count].join(" ");
        if repeated.len() > EXPAND_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let tail = current[hit.close + 1 + hit.digits..].trim();
        current = format!("{} {} {}", &current[..hit.open], repeated, tail)
            .trim()
            .to_string();
    }
    Ok(current)
}

fn strip_grouping_parens(text: &str) -> Result<String, NormalizeError> {
    let mut depth = 0i64;
    let mut out = String::with_capacity(text.len());
    for ch in text.chars() {
        match ch {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth < 0 {
                    return Err(NormalizeError::Unbalanced { alg: text.into() });
                }
            }
            _ => out.push(ch),
        }
    }
    if depth != 0 {
        return Err(NormalizeError::Unbalanced { alg: text.into() });
    }
    Ok(out)
}

fn is_auf_inner(inner: &str) -> bool {
    let tokens: Vec<&str> = inner.split_whitespace().collect();
    (1..=2).contains(&tokens.len())
        && tokens.iter().all(|token| {
            let bytes = token.as_bytes();
            matches!(bytes.first(), Some(b'U') | Some(b'D'))
                && bytes[1..]
                    .iter()
                    .all(|byte| *byte == b'2' || *byte == b'\'')
        })
}

fn valid_int(text: &str) -> bool {
    let bytes = text.as_bytes();
    let digits = text.strip_prefix('-').unwrap_or(text);
    !digits.is_empty()
        && digits.bytes().all(|byte| byte.is_ascii_digit())
        && (digits.len() == 1 || digits.as_bytes()[0] != b'0')
        && !(text.starts_with('-') && digits == "0")
        && bytes
            .iter()
            .all(|byte| byte.is_ascii_digit() || *byte == b'-')
}

fn canonical_tuple(inner: &str) -> Option<(String, bool)> {
    let (top, bottom) = inner.split_once(',')?;
    if !valid_int(top) || !valid_int(bottom) {
        return None;
    }
    let mut cells = Vec::with_capacity(2);
    let mut wrapped = false;
    for cell in [top, bottom] {
        let value: i64 = cell.parse().ok()?;
        let mapped = if value == -HALF_RING {
            HALF_RING
        } else if (TURN_MIN..=HALF_RING).contains(&value) {
            value
        } else {
            return None;
        };
        wrapped |= mapped != value;
        cells.push(mapped.to_string());
    }
    if cells.iter().all(|cell| cell == "0") {
        return None;
    }
    Some((format!("({},{})", cells[0], cells[1]), wrapped))
}

fn normalize_sq1(raw: &str) -> Result<Normalized, NormalizeError> {
    let trimmed = raw.trim();
    let mut notes = Vec::new();
    let mut text = trimmed;
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if let Some(stripped) = text.strip_suffix('*') {
        let before = stripped.trim_end();
        if before.is_empty() {
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: "*".into(),
            });
        }
        text = before;
        notes.push("equator marker stripped: *".to_string());
    }
    if text.starts_with('(')
        && let Some(close) = text.find(')')
    {
        let inner = &text[1..close];
        if is_auf_inner(inner) {
            let rest = &text[close + 1..];
            if rest.is_empty() || rest.starts_with(|ch: char| ch.is_whitespace() || ch == '(') {
                notes.push(format!("auf prefix stripped: ({inner})"));
                text = rest.trim_start();
            }
        }
    }
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    let bytes = text.as_bytes();
    let mut index = 0;
    let mut parts: Vec<String> = Vec::new();
    while index < bytes.len() {
        let byte = bytes[index];
        if byte.is_ascii_whitespace() {
            index += 1;
        } else if byte == b'/' {
            parts.push("/".to_string());
            index += 1;
        } else if byte == b'(' {
            let Some(rel) = bytes[index + 1..].iter().position(|next| *next == b')') else {
                return Err(NormalizeError::Unbalanced {
                    alg: trimmed.into(),
                });
            };
            let close = index + 1 + rel;
            let inner = &text[index + 1..close];
            let Some((tuple, wrapped)) = canonical_tuple(inner) else {
                return Err(NormalizeError::UnknownToken {
                    alg: trimmed.into(),
                    token: text[index..=close].to_string(),
                });
            };
            if wrapped {
                notes.push(format!("wraparound: ({inner}) -> {tuple}"));
            }
            parts.push(tuple);
            index = close + 1;
        } else if byte == b'-' || byte.is_ascii_digit() {
            let start = index;
            while index < bytes.len()
                && (bytes[index] == b'-' || bytes[index] == b',' || bytes[index].is_ascii_digit())
            {
                index += 1;
            }
            let token = &text[start..index];
            let Some((tuple, wrapped)) = canonical_tuple(token) else {
                return Err(NormalizeError::UnknownToken {
                    alg: trimmed.into(),
                    token: token.to_string(),
                });
            };
            if wrapped {
                notes.push(format!("wraparound: ({token}) -> {tuple}"));
            }
            parts.push(tuple);
        } else {
            let end = bytes[index..]
                .iter()
                .position(|next| next.is_ascii_whitespace() || *next == b'/' || *next == b'(')
                .map(|offset| index + offset)
                .unwrap_or(bytes.len());
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: text[index..end].to_string(),
            });
        }
    }
    Ok(Normalized {
        alg: parts.join(" "),
        notes,
    })
}
