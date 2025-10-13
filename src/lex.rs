//! lexing

use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Enumerate, Peekable};
use std::ops::Range;

use crate::format;
use crate::module::Location;

#[derive(PartialEq, Debug, Clone)]
pub enum TokenKind {
    Unknown(String),
    Number(f64),
    Bytes(Box<[u8]>),
    Word(String),
    Comma,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
    Equal,
    Def,
    Let,
    Use,
    //LineComment(String),
    //DashComment(String),
    End,
}

impl Display for TokenKind {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        match self {
            TokenKind::Unknown(tok) => write!(f, "token '{tok}'"),
            TokenKind::Number(n) => write!(f, "number '{n}'"),
            TokenKind::Bytes(s) => {
                if s.is_empty() {
                    write!(f, "empty string")
                } else if s.len() < 16 {
                    write!(f, "string '")?;
                    format::display_bytes(f, s)?;
                    write!(f, "'")
                } else {
                    write!(f, "string of {} bytes", s.len())
                }
            }
            TokenKind::Word(w) => write!(f, "word '{w}'"),
            TokenKind::Comma => write!(f, "operator ','"),
            TokenKind::OpenBracket => write!(f, "open '['"),
            TokenKind::CloseBracket => write!(f, "close ']'"),
            TokenKind::OpenBrace => write!(f, "open '{{'"),
            TokenKind::CloseBrace => write!(f, "close '}}'"),
            TokenKind::Equal => write!(f, "operator '='"),
            TokenKind::Def => write!(f, "keyword 'def'"),
            TokenKind::Let => write!(f, "keyword 'let'"),
            TokenKind::Use => write!(f, "keyword 'use'"),
            TokenKind::End => write!(f, "end of file"),
        }
    }
}

#[derive(PartialEq, Debug, Clone)]
pub struct Token(pub Location, pub TokenKind);

/// note: this is an infinite iterator (`next()` is never `None`)
pub struct Lexer<'parse, I: Iterator<Item = u8>> {
    stream: Peekable<Enumerate<I>>,
    path: &'parse str,
    last_at: usize,
}

impl<'parse, I: Iterator<Item = u8>> Lexer<'parse, I> {
    pub(crate) fn new(path: &'parse str, iter: I) -> Self {
        Self {
            stream: iter.enumerate().peekable(),
            path,
            last_at: 0,
        }
    }

    fn tok(&self, range: Range<usize>, k: TokenKind) -> Token {
        Token(Location(self.path.to_string(), range), k)
    }
}

impl<'parse, I: Iterator<Item = u8>> Iterator for Lexer<'parse, I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        use TokenKind::*;

        let Some((at, byte)) = self.stream.find(|c| !c.1.is_ascii_whitespace()) else {
            let range = self.last_at..self.last_at;
            return Some(self.tok(range, End));
        };

        let (len, tok) = match byte {
            b':' => {
                let mut doubles = 2;
                let b: Box<[u8]> =
                    iter::from_fn(|| match (self.stream.next(), self.stream.peek()) {
                        (Some((_, b':')), Some((_, b':'))) => {
                            doubles += 1;
                            self.stream.next();
                            Some(b':')
                        }
                        (Some((_, b':')), _) => None,
                        (pair, _) => pair.map(|c| c.1),
                    })
                    .collect();
                (b.len() + doubles, Bytes(b))
            }

            b',' => (1, Comma),
            b'[' => (1, OpenBracket),
            b']' => (1, CloseBracket),
            b'{' => (1, OpenBrace),
            b'}' => (1, CloseBrace),
            b'=' => (1, Equal),

            b'#' => {
                if let Some((_, b'-')) = self.stream.peek() {
                    self.stream.next();
                    let mut m = Vec::new();
                    while {
                        match self.next().expect("unreachable: infinite iterator").1 {
                            OpenBracket => m.push(b'b'),
                            CloseBracket if m.last().is_some_and(|b| b'b' == *b) => _ = m.pop(),
                            OpenBrace => m.push(b'B'),
                            CloseBrace if m.last().is_some_and(|b| b'B' == *b) => _ = m.pop(),
                            End => m.clear(),
                            _ => {}
                        }
                        !m.is_empty()
                    } {}
                    let n = self.next();
                    let Some(Token(_, Comma)) = n else {
                        return n;
                    };
                } else {
                    self.stream.find(|c| {
                        self.last_at = c.0;
                        b'\n' == c.1
                    });
                }
                return self.next();
            }

            c if c.is_ascii_lowercase() || b'-' == c => {
                let b: Vec<u8> = iter::once(c)
                    .chain(iter::from_fn(|| {
                        self.stream
                            .next_if(|c| c.1.is_ascii_lowercase() || b'-' == c.1)
                            .map(|c| c.1)
                    }))
                    .collect();
                (
                    b.len(),
                    match &b[..] {
                        b"def" => Def,
                        b"let" => Let,
                        b"use" => Use,
                        _ => Word(String::from_utf8(b).expect("unreachable: is_ascii_lowercase")),
                    },
                )
            }

            c if c.is_ascii_digit() => {
                let mut r = 0;
                let mut l = 1;

                let (shift, digits) = match (c, self.stream.peek()) {
                    (b'0', Some((_, b'B' | b'b'))) => (1, b"01".as_slice()),
                    (b'0', Some((_, b'O' | b'o'))) => (3, b"01234567".as_slice()),
                    (b'0', Some((_, b'X' | b'x'))) => (4, b"0123456789abcdef".as_slice()),
                    _ => (0, b"0123456789".as_slice()),
                };
                if 0 == shift {
                    r = c as usize - 48;
                } else {
                    l += 1;
                    self.stream.next();
                }

                while let Some(k) = self
                    .stream
                    .peek()
                    .and_then(|c| digits.iter().position(|&k| k == c.1 | 32))
                {
                    l += 1;
                    self.stream.next();
                    r = if 0 == shift {
                        r * 10 + k
                    } else {
                        (r << shift) | k
                    };
                }

                let r = if 0 == shift && self.stream.peek().is_some_and(|c| b'.' == c.1) {
                    l += 2;
                    self.stream.next();
                    let mut d = match self.stream.next() {
                        Some(c) if c.1.is_ascii_digit() => (c.1 - b'0') as usize,
                        _ => {
                            self.last_at = at + l;
                            let t = r.to_string() + ".";
                            return Some(self.tok(at..at + l, Unknown(t)));
                        }
                    };
                    let mut w = 10;
                    while let Some(c) = self.stream.peek().map(|c| c.1).filter(u8::is_ascii_digit) {
                        l += 1;
                        self.stream.next();
                        d = d * 10 + (c - b'0') as usize;
                        w *= 10;
                    }
                    r as f64 + (d as f64 / w as f64)
                } else {
                    r as f64
                };

                (l, Number(r))
            }

            c => {
                let chclass = c.is_ascii_alphanumeric() || b'_' == c;
                let b: Vec<u8> = iter::once(c)
                    .chain(iter::from_fn(|| {
                        self.stream
                            .next_if(|&(_, c)| {
                                !b":,[]{}=#-".contains(&c)
                                    && !c.is_ascii_whitespace()
                                    && (c.is_ascii_alphanumeric() || b'_' == c) == chclass
                            })
                            .map(|c| c.1)
                    }))
                    .collect();
                (b.len(), Unknown(String::from_utf8_lossy(&b).into_owned()))
            }
        };

        self.last_at = at + len;
        Some(self.tok(at..at + len, tok))
    }
}

#[test]
fn test() {
    use insta::assert_snapshot;

    fn t(script: &[u8]) -> String {
        let mut end = true;
        Lexer::new("t", script.iter().copied())
            .take_while(|t| {
                if !end {
                    false
                } else {
                    end = TokenKind::End != t.1;
                    true
                }
            })
            .fold(String::new(), |mut acc, cur| {
                acc += &format!("{cur:?}\n");
                acc
            })
    }

    assert_snapshot!(t(b""));
    assert_snapshot!(t(b"coucou"));
    assert_snapshot!(t(b"[a 1 b, {w, x, y, z}, === 0.5]"));
    assert_snapshot!(t(b":hay: :hey:: not hay: ::_:::: fin"));
    assert_snapshot!(t(b"42 :*: 0x2a 0b101010 0o52 2974382.92438732 1."));
    assert_snapshot!(t(b"oh-no; don't let use be the def of cat!"));
    assert_snapshot!(t(b"
normal
# comment
normal again
#-comment not comment
#- comment again not comment
a #- [ba ba ba] b
a #- 239084 b
a #- :yeeesldkfj: b
a #- {one, [two], [{three}]} b
a #- {one, #-[two], [{three}]} b
{heyo, #-baba, owieur} # the ',' is also commented
{heyo, baba, #-owieur} # the '}' isn't
#-{ unclosed!..
"));
}
