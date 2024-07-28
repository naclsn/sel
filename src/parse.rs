use std::io;
use std::iter;

#[derive(PartialEq, Debug)]
pub enum Token {
    Unknown(String),
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
        Some(match self.bytes.find(|c| !c.is_ascii_whitespace())? {
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
    Apply(Box<Tree>, Vec<Tree>),
    Atom(Token),
    Chain(Vec<Tree>),
    List(Vec<Tree>),
}

#[derive(PartialEq, Debug)]
pub enum Error {
    Unexpected(Token),
    UnexpectedEOF,
}

impl Tree {
    fn one_from_peekable(
        peekable: &mut iter::Peekable<Lexer<impl io::Read>>,
    ) -> Result<Tree, Error> {
        let one = match peekable.next() {
            Some(token @ (Token::Word(_) | Token::Text(_) | Token::Number(_))) => Tree::Atom(token),

            Some(Token::OpenBracket) => Tree::Chain(
                iter::once(Tree::one_from_peekable(peekable))
                    .chain(iter::from_fn(|| match peekable.next() {
                        Some(Token::Comma) => Some(Tree::one_from_peekable(peekable)),
                        Some(Token::CloseBracket) => None,
                        Some(token) => Some(Err(Error::Unexpected(token))),
                        None => Some(Err(Error::UnexpectedEOF)),
                    }))
                    .collect::<Result<_, _>>()?,
            ),

            Some(Token::OpenBrace) => Tree::List(
                iter::once(Tree::one_from_peekable(peekable))
                    .chain(iter::from_fn(|| match peekable.next() {
                        Some(Token::Comma) => Some(Tree::one_from_peekable(peekable)),
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
            _ => Some(Tree::one_from_peekable(peekable)),
        })
        .collect::<Result<_, _>>()?;
        Ok(if args.is_empty() {
            one
        } else {
            Tree::Apply(Box::new(one), args)
        })
    }

    fn from_peekable(peekable: &mut iter::Peekable<Lexer<impl io::Read>>) -> Result<Tree, Error> {
        let mut r = vec![Tree::one_from_peekable(peekable)?];
        while let Some(Token::Comma) = peekable.next() {
            r.push(Tree::one_from_peekable(peekable)?);
        }
        Ok(Tree::Chain(r))
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lexing() {
        assert_eq!(
            &[Token::Word("coucou".into()),][..],
            Lexer::new("coucou".as_bytes()).collect::<Vec<Token>>()
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
            Lexer::new("[a 1 b, {w, x, y, z}]".as_bytes()).collect::<Vec<Token>>()
        );

        assert_eq!(
            &[
                Token::Number(42),
                Token::Text(vec![42]),
                Token::Number(42),
                Token::Number(42),
                Token::Number(42),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".as_bytes()).collect::<Vec<Token>>()
        );
    }

    #[test]
    fn parsing() {
        assert_eq!(
            Ok(Tree::Chain(vec![Tree::Atom(Token::Word("coucou".to_string()))])),
            Tree::from_str("coucou")
        );

        assert_eq!(
            Ok(Tree::Chain(vec![
                Tree::Apply(
                    Box::new(Tree::Atom(Token::Word("a".to_string()))),
                    vec![Tree::Apply(
                        Box::new(Tree::Atom(Token::Number(1))),
                        vec![Tree::Atom(Token::Word("b".to_string()))],
                    )],
                ),
                Tree::List(vec![
                    Tree::Atom(Token::Word("w".to_string())),
                    Tree::Atom(Token::Word("x".to_string())),
                    Tree::Atom(Token::Word("y".to_string())),
                    Tree::Atom(Token::Word("z".to_string())),
                ])
            ])),
            Tree::from_str("a 1 b, {w, x, y, z}")
        );

        // todo
        //assert_eq!(Err...)
    }
}
