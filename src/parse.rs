use std::fmt::{Display, Formatter, Result as FmtResult};
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

pub(crate) struct Lexer<I: Iterator<Item = u8>>(iter::Peekable<I>);

impl<I: Iterator<Item = u8>> Lexer<I> {
    fn new(iter: I) -> Self {
        Self(iter.peekable())
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        Some(match self.0.find(|c| !c.is_ascii_whitespace())? {
            b':' => Token::Bytes({
                iter::from_fn(|| match (self.0.next(), self.0.peek()) {
                    (Some(b':'), Some(b':')) => {
                        let _ = self.0.next();
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

            b'#' => self.0.find(|&c| b'\n' == c).and_then(|_| self.next())?,

            c if c.is_ascii_lowercase() => Token::Word(
                String::from_utf8(
                    iter::once(c)
                        .chain(iter::from_fn(|| self.0.next_if(u8::is_ascii_lowercase)))
                        .collect(),
                )
                .unwrap(),
            ),

            c if c.is_ascii_digit() => Token::Number({
                let mut r = 0i32;
                let (shift, digits) = match (c, self.0.peek()) {
                    (b'0', Some(b'B' | b'b')) => {
                        self.0.next();
                        (1, b"01".as_slice())
                    }
                    (b'0', Some(b'O' | b'o')) => {
                        self.0.next();
                        (3, b"01234567".as_slice())
                    }
                    (b'0', Some(b'X' | b'x')) => {
                        self.0.next();
                        (4, b"0123456789abcdef".as_slice())
                    }
                    _ => {
                        r = c as i32 - 48;
                        (0, b"0123456789".as_slice())
                    }
                };
                while let Some(k) = self
                    .0
                    .peek()
                    .and_then(|&c| digits.iter().position(|&k| k == c | 32).map(|k| k as i32))
                {
                    self.0.next();
                    r = if 0 == shift {
                        r * 10 + k
                    } else {
                        r << shift | k
                    }
                }
                r
            }),

            c => Token::Unknown(
                String::from_utf8_lossy(
                    &iter::once(c)
                        .chain(self.0.by_ref().take_while(|c| !c.is_ascii_whitespace()))
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
        peekable: &mut iter::Peekable<Lexer<impl Iterator<Item = u8>>>,
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
        peekable: &mut iter::Peekable<Lexer<impl Iterator<Item = u8>>>,
        types: &mut Types,
    ) -> Result<Tree, Error> {
        let mut r = Tree::one_from_peekable(peekable, types)?;
        while let Some(Token::Comma) = peekable.next() {
            r = Tree::one_from_peekable(peekable, types)?.try_apply(r, types)?;
        }
        Ok(r)
    }

    fn from_lexer(
        lexer: Lexer<impl Iterator<Item = u8>>,
        types: &mut Types,
    ) -> Result<Tree, Error> {
        Tree::from_peekable(&mut lexer.peekable(), types)
    }

    pub fn new(iter: impl IntoIterator<Item = u8>, types: &mut Types) -> Result<Tree, Error> {
        Tree::from_lexer(Lexer::new(iter.into_iter()), types)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lexing() {
        assert_eq!(
            &[Token::Word("coucou".into()),][..],
            Lexer::new("coucou".bytes()).collect::<Vec<_>>()
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
            Lexer::new("[a 1 b, {w, x, y, z}]".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token::Number(42),
                Token::Bytes(vec![42]),
                Token::Number(42),
                Token::Number(42),
                Token::Number(42),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        assert_eq!(
            Ok(Tree::Atom(Token::Word("coucou".to_string()))),
            Tree::new("coucou".bytes(), &mut Types::new())
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
            Tree::new(
                "input, split:-:, map[add1], join:+:".bytes(),
                &mut Types::new()
            )
        );

        // todo
        //assert_eq!(Err...)
    }

    #[test]
    fn typing() {
        // todo
    }
}
