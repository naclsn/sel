use std::io;
use std::iter;

#[derive(Debug)]
pub enum Token {
    Word(String),
    Text(Vec<u8>),
    Number(i32),
    Comma,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
}

struct Bytes<T: io::Read> {
    inner: io::Bytes<T>,
}

impl<T: io::Read> Iterator for Bytes<T> {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().and_then(|e| e.ok())
    }
}

pub(crate) struct Lexer<T: io::Read> {
    bytes: iter::Peekable<Bytes<T>>,
}

impl<T: io::Read> Lexer<T> {
    fn new(stream: T) -> Self {
        Self {
            bytes: Bytes {
                inner: stream.bytes(),
            }
            .peekable(),
        }
    }
}

impl<T: io::Read> Iterator for Lexer<T> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(first) = self.bytes.find(|c| !c.is_ascii_whitespace()) {
            Some(match first {
                b':' => Token::Text({
                    iter::from_fn(|| match (self.bytes.next(), self.bytes.peek()) {
                        (Some(b':'), Some(b':')) => {
                            let _ = self.bytes.next();
                            Some(b':')
                        }
                        (Some(b':'), _) => None,
                        (c, _) => c,
                    })
                    .collect()
                }),

                b',' => Token::Comma,
                b'[' => Token::OpenBracket,
                b']' => Token::CloseBracket,
                b'{' => Token::OpenBrace,
                b'}' => Token::CloseBrace,

                c => {
                    if c.is_ascii_lowercase() {
                        Token::Word(
                            String::from_utf8(
                                iter::once(c)
                                    .chain(iter::from_fn(|| {
                                        self.bytes.next_if(u8::is_ascii_lowercase)
                                    }))
                                    .collect(),
                            )
                            .unwrap(),
                        )
                    } else if c.is_ascii_digit() {
                        Token::Number({
                            let mut r = 0i32;
                            let (shift, digits) = match (c, self.bytes.peek()) {
                                (b'0', Some(b'B' | b'b')) => {
                                    let _ = self.bytes.next();
                                    (1, b"01".as_slice())
                                }
                                (b'0', Some(b'O' | b'o')) => {
                                    let _ = self.bytes.next();
                                    (3, b"01234567".as_slice())
                                }
                                (b'0', Some(b'X' | b'x')) => {
                                    let _ = self.bytes.next();
                                    (4, b"0123456789abcdef".as_slice())
                                }
                                _ => {
                                    r = c as i32 - 48;
                                    (0, b"0123456789".as_slice())
                                }
                            };
                            while let Some(c) = self.bytes.peek() {
                                let Some(k) = digits.iter().position(|&x| c | 32 == x) else {
                                    break;
                                };
                                let _ = self.bytes.next();
                                if 0 == shift {
                                    r = r * 10 + k as i32;
                                } else {
                                    r = r << shift | k as i32;
                                }
                            }
                            r
                        })
                    } else {
                        return None;
                    }
                }
            })
        } else {
            None
        }
    }
}

#[derive(Debug)]
pub enum Tree {
    Apply(Box<Tree>, Vec<Tree>),
    Atom(Token),
    Subsc(Vec<Tree>),
    List(Vec<Tree>),
}

#[derive(Debug)]
pub enum Error {
    Unexpected(Token),
    UnexpectedEOF,
}

impl Tree {
    fn from_peekable(peekable: &mut iter::Peekable<Lexer<impl io::Read>>) -> Result<Tree, Error> {
        let one = match peekable.next() {
            Some(token @ (Token::Word(_) | Token::Text(_) | Token::Number(_))) => Tree::Atom(token),

            Some(Token::OpenBracket) => Tree::Subsc(
                iter::once(Tree::from_peekable(peekable))
                    .chain(iter::from_fn(|| match peekable.next() {
                        Some(Token::Comma) => Some(Tree::from_peekable(peekable)),
                        Some(Token::CloseBracket) => None,
                        Some(token) => Some(Err(Error::Unexpected(token))),
                        None => Some(Err(Error::UnexpectedEOF)),
                    }))
                    .collect::<Result<_, _>>()?,
            ),

            Some(Token::OpenBrace) => Tree::List(
                iter::once(Tree::from_peekable(peekable))
                    .chain(iter::from_fn(|| match peekable.next() {
                        Some(Token::Comma) => Some(Tree::from_peekable(peekable)),
                        Some(Token::CloseBrace) => None,
                        Some(token) => Some(Err(Error::Unexpected(token))),
                        None => Some(Err(Error::UnexpectedEOF)),
                    }))
                    .collect::<Result<_, _>>()?,
            ),

            Some(token) => return Err(Error::Unexpected(token)),
            None => return Err(Error::UnexpectedEOF),
        };

        let args: Vec<_> = iter::from_fn(|| match peekable.peek() {
            Some(Token::Comma | Token::CloseBracket | Token::CloseBrace) | None => None,
            _ => Some(Tree::from_peekable(peekable)),
        })
        .collect::<Result<_, _>>()?;
        Ok(if args.is_empty() {
            one
        } else {
            Tree::Apply(Box::new(one), args)
        })
    }

    pub fn from_lexer(lexer: Lexer<impl io::Read>) -> Result<Tree, Error> {
        Tree::from_peekable(&mut lexer.peekable())
    }

    pub fn from_stream(stream: impl io::Read) -> Result<Tree, Error> {
        Tree::from_lexer(Lexer::new(stream))
    }

    pub fn from_str(text: &str) -> Result<Tree, Error> {
        Tree::from_stream(text.as_bytes())
    }
}
