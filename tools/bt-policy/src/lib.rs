pub mod comments;
pub mod constants;
pub mod lexer;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Finding {
    pub path: String,
    pub line: usize,
    pub kind: &'static str,
    pub detail: String,
}
