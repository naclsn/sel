use std::fmt::{Display, Formatter, Result as FmtResult};
use std::io;
use std::iter;

use crate::builtin::lookup_type;
use crate::typing::{Type, Types};

#[derive(PartialEq, Debug)]
pub enum Token {
    Unknown(String),
    Word(String),
    Bytes(Vec<u8>),
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
        Some(match self.bytes.find(|c| !c.is_ascii_whitespace())? {
            b':' => Token::Bytes({
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

            b'#' => self.bytes.find(|&c| b'\n' == c).and_then(|_| self.next())?,

            c if c.is_ascii_lowercase() => Token::Word(
                String::from_utf8(
                    iter::once(c)
                        .chain(iter::from_fn(|| self.bytes.next_if(u8::is_ascii_lowercase)))
                        .collect(),
                )
                .unwrap(),
            ),

            c if c.is_ascii_digit() => Token::Number({
                let mut r = 0i32;
                let (shift, digits) = match (c, self.bytes.peek()) {
                    (b'0', Some(b'B' | b'b')) => {
                        self.bytes.next();
                        (1, b"01".as_slice())
                    }
                    (b'0', Some(b'O' | b'o')) => {
                        self.bytes.next();
                        (3, b"01234567".as_slice())
                    }
                    (b'0', Some(b'X' | b'x')) => {
                        self.bytes.next();
                        (4, b"0123456789abcdef".as_slice())
                    }
                    _ => {
                        r = c as i32 - 48;
                        (0, b"0123456789".as_slice())
                    }
                };
                while let Some(k) = self
                    .bytes
                    .peek()
                    .and_then(|&c| digits.iter().position(|&k| k == c | 32))
                {
                    self.bytes.next();
                    r = if 0 == shift {
                        r * 10 + k as i32
                    } else {
                        r << shift | k as i32
                    }
                }
                r
            }),

            c => Token::Unknown(
                String::from_utf8_lossy(
                    &iter::once(c)
                        .chain(self.bytes.by_ref().take_while(|c| !c.is_ascii_whitespace()))
                        .collect::<Vec<_>>(),
                )
                .into_owned(),
            ),
        })
    }
}

#[derive(PartialEq, Debug)]
pub enum Tree {
    Atom(Token),
    List(Vec<Tree>),
    Apply(usize, String, Vec<Tree>),
}

impl Display for Tree {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            Tree::Atom(atom) => match atom {
                Token::Word(w) => write!(f, "{w}"),
                Token::Bytes(v) => write!(f, "{v:?}"),
                Token::Number(n) => write!(f, "{n}"),
                _ => unreachable!(),
            },

            Tree::List(items) => {
                write!(f, "{{")?;
                let mut sep = "";
                for it in items {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, "}}")
            }

            Tree::Apply(_, name, args) => {
                write!(f, "{name}(")?;
                let mut sep = "";
                for it in args {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, ")")
            }
        }
    }
}

#[derive(PartialEq, Debug)]
pub enum Error {
    Unexpected(Token),
    UnexpectedEOF,

    UnknownName(String),
    NotFunc(usize, Types),
    ExpectedButGot(usize, usize, Types),
    InfWhereFinExpected,
}

impl Tree {
    pub fn try_apply(self, arg: Tree, types: &mut Types) -> Result<Tree, Error> {
        let (ty, name, mut args) = match self {
            Tree::Atom(Token::Word(name)) => (lookup_type(&name, types)?, name, vec![]),
            Tree::Apply(ty, name, args) => (ty, name, args),
            _ => {
                return Err(Error::NotFunc(
                    self.make_type(types).unwrap(),
                    types.clone(),
                ))
            }
        };

        let ty = Type::applied(ty, arg.make_type(types)?, types)?;
        args.push(arg);
        Ok(Tree::Apply(ty, name, args))
    }

    pub fn make_type(&self, types: &mut Types) -> Result<usize, Error> {
        match self {
            Tree::Atom(atom) => match atom {
                Token::Word(name) => lookup_type(name, types),
                Token::Bytes(_) => Ok(1),
                Token::Number(_) => Ok(0),
                _ => unreachable!(),
            },

            Tree::List(_items) => todo!(),

            Tree::Apply(ty, _, _) => Ok(*ty),
        }
    }

    fn one_from_peekable(
        peekable: &mut iter::Peekable<Lexer<impl io::Read>>,
        types: &mut Types,
    ) -> Result<Tree, Error> {
        let mut r = match peekable.next() {
            Some(token @ (Token::Word(_) | Token::Bytes(_) | Token::Number(_))) => {
                Tree::Atom(token)
            }

            Some(Token::OpenBracket) => {
                let mut rr = Tree::one_from_peekable(peekable, types)?;
                loop {
                    match peekable.next() {
                        Some(Token::Comma) => {
                            rr = Tree::one_from_peekable(peekable, types)?.try_apply(rr, types)?
                        }
                        Some(Token::CloseBracket) => break rr,

                        Some(token) => return Err(Error::Unexpected(token)),
                        None => return Err(Error::UnexpectedEOF),
                    }
                }
            }

            Some(Token::OpenBrace) => Tree::List(
                // XXX: doesn't allow empty, this is half intentional
                iter::once(Tree::one_from_peekable(peekable, types))
                    .chain(iter::from_fn(|| match peekable.next() {
                        Some(Token::Comma) => Some(Tree::one_from_peekable(peekable, types)),
                        Some(Token::CloseBrace) => None,
                        Some(token) => Some(Err(Error::Unexpected(token))),
                        None => Some(Err(Error::UnexpectedEOF)),
                    }))
                    .collect::<Result<_, _>>()?,
            ),

            Some(token) => return Err(Error::Unexpected(token)),
            None => return Err(Error::UnexpectedEOF),
        };

        while !matches!(
            peekable.peek(),
            Some(Token::Comma | Token::CloseBracket | Token::CloseBrace) | None
        ) {
            r = r.try_apply(Tree::one_from_peekable(peekable, types)?, types)?;
        }
        Ok(r)
    }

    fn from_peekable(
        peekable: &mut iter::Peekable<Lexer<impl io::Read>>,
        types: &mut Types,
    ) -> Result<Tree, Error> {
        let mut r = Tree::one_from_peekable(peekable, types)?;
        while let Some(Token::Comma) = peekable.next() {
            r = Tree::one_from_peekable(peekable, types)?.try_apply(r, types)?;
        }
        Ok(r)
    }

    pub fn from_lexer(lexer: Lexer<impl io::Read>, types: &mut Types) -> Result<Tree, Error> {
        Tree::from_peekable(&mut lexer.peekable(), types)
    }

    pub fn from_stream(stream: impl io::Read, types: &mut Types) -> Result<Tree, Error> {
        Tree::from_lexer(Lexer::new(stream), types)
    }

    pub fn from_str(text: &str, types: &mut Types) -> Result<Tree, Error> {
        Tree::from_stream(text.as_bytes(), types)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lexing() {
        assert_eq!(
            &[Token::Word("coucou".into()),][..],
            Lexer::new("coucou".as_bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token::OpenBracket,
                Token::Word("a".to_string()),
                Token::Number(1),
                Token::Word("b".to_string()),
                Token::Comma,
                Token::OpenBrace,
                Token::Word("w".to_string()),
                Token::Comma,
                Token::Word("x".to_string()),
                Token::Comma,
                Token::Word("y".to_string()),
                Token::Comma,
                Token::Word("z".to_string()),
                Token::CloseBrace,
                Token::CloseBracket,
            ][..],
            Lexer::new("[a 1 b, {w, x, y, z}]".as_bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token::Number(42),
                Token::Bytes(vec![42]),
                Token::Number(42),
                Token::Number(42),
                Token::Number(42),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".as_bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        assert_eq!(
            Ok(Tree::Atom(Token::Word("coucou".to_string()))),
            Tree::from_str("coucou", &mut Types::new())
        );

        assert_eq!(
            Ok(Tree::Apply(
                0,
                "join".to_string(),
                vec![
                    Tree::Atom(Token::Bytes(vec![43])),
                    Tree::Apply(
                        0,
                        "map".to_string(),
                        vec![
                            Tree::Apply(0, "add".to_string(), vec![Tree::Atom(Token::Number(1))],),
                            Tree::Apply(
                                0,
                                "split".to_string(),
                                vec![
                                    Tree::Atom(Token::Bytes(vec![45])),
                                    Tree::Atom(Token::Word("input".to_string())),
                                ],
                            ),
                        ],
                    ),
                ],
            )),
            Tree::from_str("input, split:-:, map[add1], join:+:", &mut Types::new())
        );

        // todo
        //assert_eq!(Err...)
    }

    #[test]
    fn typing() {
        // todo
    }
}
