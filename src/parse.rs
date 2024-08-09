use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin::lookup_type;
use crate::error::{Error, ErrorContext, ErrorList};
use crate::types::{self, Type, TypeList, TypeRef};

pub const COMPOSE_OP_FUNC_NAME: &str = "(,)";
const UNKNOWN_NAME: &str = "{unknown}";

#[derive(PartialEq, Debug, Clone)]
pub enum Token {
    Unknown(String),
    Word(String),
    Bytes(Vec<u8>),
    Number(f64),
    Comma,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
}

#[derive(PartialEq, Debug)]
pub enum Tree {
    Bytes(Vec<u8>),
    Number(f64),
    List(TypeRef, Vec<Tree>),
    Apply(TypeRef, String, Vec<Tree>),
}

// lexing into tokens {{{
struct Lexer<I: Iterator<Item = u8>> {
    stream: iter::Peekable<I>,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    fn new(iter: I) -> Self {
        Self {
            stream: iter.peekable(),
        }
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        Some(match self.stream.find(|c| !c.is_ascii_whitespace())? {
            b':' => Token::Bytes({
                iter::from_fn(|| match (self.stream.next(), self.stream.peek()) {
                    (Some(b':'), Some(b':')) => {
                        self.stream.next();
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

            b'#' => self
                .stream
                .find(|&c| b'\n' == c)
                .and_then(|_| self.next())?,

            c if c.is_ascii_lowercase() => Token::Word(
                String::from_utf8(
                    iter::once(c)
                        .chain(iter::from_fn(|| {
                            self.stream.next_if(u8::is_ascii_lowercase)
                        }))
                        .collect(),
                )
                .unwrap(),
            ),

            c if c.is_ascii_digit() => Token::Number({
                let mut r = 0usize;
                let (shift, digits) = match (c, self.stream.peek()) {
                    (b'0', Some(b'B' | b'b')) => {
                        self.stream.next();
                        (1, b"01".as_slice())
                    }
                    (b'0', Some(b'O' | b'o')) => {
                        self.stream.next();
                        (3, b"01234567".as_slice())
                    }
                    (b'0', Some(b'X' | b'x')) => {
                        self.stream.next();
                        (4, b"0123456789abcdef".as_slice())
                    }
                    _ => {
                        r = c as usize - 48;
                        (0, b"0123456789".as_slice())
                    }
                };
                while let Some(k) = self
                    .stream
                    .peek()
                    .and_then(|&c| digits.iter().position(|&k| k == c | 32))
                {
                    self.stream.next();
                    r = if 0 == shift {
                        r * 10 + k
                    } else {
                        r << shift | k
                    };
                }
                if 0 == shift && self.stream.peek().is_some_and(|&c| b'.' == c) {
                    self.stream.next();
                    let mut d = match self.stream.next() {
                        Some(c) if c.is_ascii_digit() => (c - b'0') as usize,
                        _ => return Some(Token::Unknown(r.to_string() + ".")),
                    };
                    let mut w = 10usize;
                    while let Some(c) = self.stream.peek().copied().filter(u8::is_ascii_digit) {
                        self.stream.next();
                        d = d * 10 + (c - b'0') as usize;
                        w *= 10;
                    }
                    r as f64 + (d as f64 / w as f64)
                } else {
                    r as f64
                }
            }),

            c => Token::Unknown(
                String::from_utf8_lossy(
                    &iter::once(c)
                        .chain(
                            self.stream
                                .by_ref()
                                .take_while(|c| !c.is_ascii_whitespace()),
                        )
                        .collect::<Vec<_>>(),
                )
                .into_owned(),
            ),
        })
    }
}
// }}}

// parsing into tree {{{
struct Parser<I: Iterator<Item = u8>> {
    peekable: iter::Peekable<Lexer<I>>,
    types: TypeList,
    errors: Vec<Error>,
}

impl<I: Iterator<Item = u8>> Parser<I> {
    fn new(iter: I) -> Parser<I> {
        Parser {
            peekable: Lexer::new(iter.into_iter()).peekable(),
            types: TypeList::default(),
            errors: Vec::new(),
        }
    }

    fn report(&mut self, err: Error) {
        self.errors.push(err);
    }
    fn report_caused(&mut self, err: Error, because: ErrorContext) {
        self.errors.push(Error::ContextCaused {
            error: Box::new(err),
            because,
        })
    }

    fn parse_value(&mut self) -> Tree {
        let mkerr = |types: &mut TypeList| {
            Tree::Apply(
                types.named(types::ERROR_TYPE),
                UNKNOWN_NAME.to_string(),
                Vec::new(),
            )
        };

        if let Some(token @ Token::CloseBracket) | Some(token @ Token::CloseBrace) =
            self.peekable.peek()
        {
            let token = token.clone();
            self.report(Error::Unexpected {
                token,
                expected: "a value",
            });
            return mkerr(&mut self.types);
        }

        match self.peekable.next() {
            Some(Token::Word(w)) => match lookup_type(&w, &mut self.types) {
                Some(ty) => Tree::Apply(ty, w, Vec::new()),
                None => {
                    self.report(Error::UnknownName(w));
                    mkerr(&mut self.types)
                }
            },

            Some(Token::Bytes(b)) => Tree::Bytes(b),
            Some(Token::Number(n)) => Tree::Number(n),

            Some(open_token @ Token::OpenBracket) => {
                let rr = self.parse_script();
                match self.peekable.peek() {
                    Some(Token::CloseBracket) => _ = self.peekable.next(),
                    Some(token) => {
                        let token = token.clone();
                        if let Token::Comma = token {
                            self.peekable.next();
                        }
                        self.report_caused(
                            Error::Unexpected {
                                token,
                                expected: "next argument or closing ']'",
                            },
                            ErrorContext::Unmatched(open_token),
                        )
                    }
                    None => self
                        .report_caused(Error::UnexpectedEnd, ErrorContext::Unmatched(open_token)),
                }
                rr
            }

            Some(open_token @ Token::OpenBrace) => {
                let mut items = Vec::new();
                let ty = self.types.named("a");
                if let Some(Token::CloseBrace) = self.peekable.peek() {
                    self.peekable.next();
                } else {
                    loop {
                        let item = self.parse_apply();
                        Type::harmonize(ty, item.get_type(), &mut self.types).unwrap_or_else(|e| {
                            self.report_caused(e, ErrorContext::TypeListInferred(0 /* TODO */))
                        });
                        items.push(item);
                        match self.peekable.next() {
                            Some(Token::CloseBrace) => (),
                            Some(Token::Comma) => {
                                if let Some(Token::CloseBrace) = self.peekable.peek() {
                                    self.peekable.next();
                                } else {
                                    continue;
                                }
                            }
                            Some(token) => self.report_caused(
                                Error::Unexpected {
                                    token,
                                    expected: "next item or closing '}'",
                                },
                                ErrorContext::Unmatched(open_token),
                            ),
                            None => self.report_caused(
                                Error::UnexpectedEnd,
                                ErrorContext::Unmatched(open_token),
                            ),
                        }
                        break;
                    }
                }
                Tree::List(self.types.list(self.types.finite(), ty), items)
            }

            Some(token) => {
                self.report(Error::Unexpected {
                    token,
                    expected: "a value",
                });
                mkerr(&mut self.types)
            }
            None => {
                self.report(Error::UnexpectedEnd);
                mkerr(&mut self.types)
            }
        }
    }

    fn parse_apply(&mut self) -> Tree {
        let mut r = self.parse_value();
        while !matches!(
            self.peekable.peek(),
            Some(Token::Comma | Token::CloseBracket | Token::CloseBrace) | None
        ) {
            // "unfold" means as in this example:
            // [tonum, add 1, tostr] :42:
            //  `-> (,)((,)(tonum, add(1)), tostr)
            // `-> (,)((,)(tonum(:[52, 50]:), add(1)), tostr)
            // => tostr(add(1, tonum(:[52, 50]:)))
            fn apply_maybe_unfold(
                p: &mut Parser<impl Iterator<Item = u8>>,
                mut func: Tree,
            ) -> Tree {
                match func {
                    Tree::Apply(_, ref name, args) if COMPOSE_OP_FUNC_NAME == name => {
                        let mut args = args.into_iter();
                        let (f, mut g) = (args.next().unwrap(), args.next().unwrap());
                        // (,)(f, g) => g(f(..))
                        g.try_apply(apply_maybe_unfold(p, f), &mut p.types)
                            .unwrap_or_else(|e| p.report(e)); // XXX: "because arg to.."
                        g
                    }
                    _ => {
                        func.try_apply(p.parse_value(), &mut p.types)
                            .unwrap_or_else(|e| p.report(e)); // XXX: "because arg to.."
                        func
                    }
                }
            }
            r = apply_maybe_unfold(self, r);
        }
        r
    }

    fn parse_script(&mut self) -> Tree {
        let mut r = self.parse_apply();
        if let Some(Token::Comma) = self.peekable.peek() {
            self.peekable.next();
            let mut then = self.parse_apply();
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            if Type::applicable(then.get_type(), r.get_type(), &self.types) {
                while let Some(Token::Comma) = {
                    then.try_apply(r, &mut self.types)
                        .unwrap_or_else(|e| self.report(e)); // XXX: "because arg to.."
                    r = then;
                    self.peekable.peek()
                } {
                    self.peekable.next();
                    then = self.parse_apply();
                }
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                while let Some(Token::Comma) = {
                    let mut compose = Tree::Apply(
                        {
                            // (,) :: (a -> b) -> (b -> c) -> a -> c
                            let a = self.types.named("a");
                            let b = self.types.named("b");
                            let c = self.types.named("c");
                            let ab = self.types.func(a, b);
                            let bc = self.types.func(b, c);
                            let ac = self.types.func(a, c);
                            let ret = self.types.func(bc, ac);
                            self.types.func(ab, ret)
                        },
                        COMPOSE_OP_FUNC_NAME.into(),
                        Vec::new(),
                    );
                    compose
                        .try_apply(r, &mut self.types)
                        .unwrap_or_else(|e| self.report(e)); // XXX: "because arg to.."
                    compose
                        .try_apply(then, &mut self.types)
                        .unwrap_or_else(|e| self.report(e)); // XXX: "because arg to.."
                    r = compose;
                    self.peekable.peek()
                } {
                    self.peekable.next();
                    then = self.parse_apply();
                }
            }
        }
        r
    }
}
// }}}

// uuhh.. tree itself (the entry point, Tree::new) {{{
impl Tree {
    fn try_apply(&mut self, arg: Tree, types: &mut TypeList) -> Result<(), Error> {
        let niw = Type::applied(self.get_type(), arg.get_type(), types)?;
        if let Tree::Apply(ty, _, args) = self {
            *ty = niw;
            args.push(arg);
        } // XXX: maybe else { unreachable!(); }
        Ok(())
    }

    fn get_type(&self) -> TypeRef {
        match self {
            Tree::Bytes(_) => types::STRFIN_TYPEREF,
            Tree::Number(_) => types::NUMBER_TYPEREF,

            Tree::List(ty, _) => *ty,

            Tree::Apply(ty, _, _) => *ty,
        }
    }

    #[allow(dead_code)]
    pub fn new(iter: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        if p.errors.is_empty() {
            Ok(r)
        } else {
            Err(ErrorList(p.errors, p.types))
        }
    }

    #[allow(dead_code)]
    pub fn new_typed(
        iter: impl IntoIterator<Item = u8>,
    ) -> Result<(TypeRef, TypeList, Tree), ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        if p.errors.is_empty() {
            Ok((r.get_type(), p.types, r))
        } else {
            Err(ErrorList(p.errors, p.types))
        }
    }
}

impl Display for Tree {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            Tree::Bytes(v) => write!(f, ":{v:?}:"),
            Tree::Number(n) => write!(f, "{n}"),

            Tree::List(_, items) => {
                write!(f, "{{")?;
                let mut sep = "";
                for it in items {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, "}}")
            }

            Tree::Apply(_, w, empty) if empty.is_empty() => write!(f, "{w}"),
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
// }}}

// tests {{{
#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::FrozenType;

    fn expect<T>(r: Result<T, ErrorList>) -> T {
        r.unwrap_or_else(|e| {
            e.crud_report();
            panic!("errors above")
        })
    }

    fn compare(left: &Tree, right: &Tree) -> bool {
        match (left, right) {
            (Tree::List(_, la), Tree::List(_, ra)) => la
                .iter()
                .zip(ra.iter())
                .find(|(l, r)| !compare(l, r))
                .is_none(),
            (Tree::Apply(_, ln, la), Tree::Apply(_, rn, ra))
                if ln == rn && la.len() == ra.len() =>
            {
                la.iter()
                    .zip(ra.iter())
                    .find(|(l, r)| !compare(l, r))
                    .is_none()
            }
            (l, r) => *l == *r,
        }
    }

    macro_rules! assert_tree {
        ($left:expr, $right:expr) => {
            match (&$left, &$right) {
                (l, r) if !compare(l, r) => assert_eq!(l, r),
                _ => (),
            }
        };
    }

    #[test]
    fn lexing() {
        use Token::*;

        assert_eq!(
            &[Word("coucou".into()),][..],
            Lexer::new("coucou".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                OpenBracket,
                Word("a".into()),
                Number(1.0),
                Word("b".into()),
                Comma,
                OpenBrace,
                Word("w".into()),
                Comma,
                Word("x".into()),
                Comma,
                Word("y".into()),
                Comma,
                Word("z".into()),
                CloseBrace,
                Comma,
                Number(0.5),
                CloseBracket,
            ][..],
            Lexer::new("[a 1 b, {w, x, y, z}, 0.5]".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Bytes(vec![104, 97, 121]),
                Bytes(vec![104, 101, 121, 58, 32, 110, 111, 116, 32, 104, 97, 121]),
                Bytes(vec![]),
                Bytes(vec![58]),
                Word("fin".into()),
            ][..],
            Lexer::new(":hay: :hey:: not hay: :: :::: fin".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Number(42.0),
                Bytes(vec![42]),
                Number(42.0),
                Number(42.0),
                Number(42.0),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        use Tree::*;

        let (ty, types, res) = expect(Tree::new_typed("input".bytes()));
        assert_tree!(Apply(0, "input".into(), vec![]), res);
        assert_eq!(FrozenType::Bytes(false), types.frozen(ty));

        let (ty, types, res) = expect(Tree::new_typed(
            "input, split:-:, map[tonum, add1, tostr], join:+:".bytes(),
        ));
        assert_tree!(
            Apply(
                0,
                "join".into(),
                vec![
                    Bytes(vec![43]),
                    Apply(
                        0,
                        "map".into(),
                        vec![
                            Apply(
                                0,
                                COMPOSE_OP_FUNC_NAME.into(),
                                vec![
                                    Apply(
                                        0,
                                        COMPOSE_OP_FUNC_NAME.into(),
                                        vec![
                                            Apply(0, "tonum".into(), vec![]),
                                            Apply(0, "add".into(), vec![Number(1.0)])
                                        ]
                                    ),
                                    Apply(0, "tostr".into(), vec![])
                                ]
                            ),
                            Apply(
                                0,
                                "split".into(),
                                vec![Bytes(vec![45]), Apply(0, "input".into(), vec![]),],
                            ),
                        ],
                    ),
                ],
            ),
            res
        );
        assert_eq!(
            FrozenType::Bytes(true /* FIXME: this should be false, right? */),
            types.frozen(ty)
        );

        let (ty, types, res) = expect(Tree::new_typed("tonum, add234121, tostr, ln".bytes()));
        assert_tree!(
            Apply(
                0,
                COMPOSE_OP_FUNC_NAME.into(),
                vec![
                    Apply(
                        0,
                        COMPOSE_OP_FUNC_NAME.into(),
                        vec![
                            Apply(
                                0,
                                COMPOSE_OP_FUNC_NAME.into(),
                                vec![
                                    Apply(0, "tonum".into(), vec![]),
                                    Apply(0, "add".into(), vec![Number(234121.0)])
                                ]
                            ),
                            Apply(0, "tostr".into(), vec![])
                        ]
                    ),
                    Apply(0, "ln".into(), vec![])
                ]
            ),
            res
        );
        assert_eq!(
            FrozenType::Func(
                Box::new(FrozenType::Bytes(false)),
                Box::new(FrozenType::Bytes(true))
            ),
            types.frozen(ty)
        );

        let (ty, types, res) = expect(Tree::new_typed("[tonum, add234121, tostr] :13242:".bytes()));
        assert_tree!(
            Apply(
                0,
                "tostr".into(),
                vec![Apply(
                    0,
                    "add".into(),
                    vec![
                        Number(234121.0),
                        Apply(0, "tonum".into(), vec![Bytes(vec![49, 51, 50, 52, 50])])
                    ]
                )]
            ),
            res
        );
        assert_eq!(FrozenType::Bytes(true), types.frozen(ty));
    }
}
// }}}
