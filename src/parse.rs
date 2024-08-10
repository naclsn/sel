use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin::lookup_type;
use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::types::{self, FrozenType, Type, TypeList, TypeRef};

pub const COMPOSE_OP_FUNC_NAME: &str = "(,)";
const UNKNOWN_NAME: &str = "{unknown}";

#[derive(PartialEq, Debug, Clone, Copy)]
pub struct Location(usize);

#[derive(PartialEq, Debug, Clone)]
pub enum TokenKind {
    Unknown(String),
    Word(String),
    Bytes(Vec<u8>),
    Number(f64),
    Comma,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
    End,
}

#[derive(PartialEq, Debug, Clone)]
pub struct Token(pub Location, pub TokenKind);

#[derive(PartialEq, Debug)]
pub enum TreeKind {
    Bytes(Vec<u8>),
    Number(f64),
    List(TypeRef, Vec<Tree>),
    Apply(TypeRef, String, Vec<Tree>),
}

#[derive(PartialEq, Debug)]
pub struct Tree(pub Location, pub TreeKind);

// lexing into tokens {{{
struct Lexer<I: Iterator<Item = u8>> {
    stream: iter::Peekable<iter::Enumerate<I>>,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    fn new(iter: I) -> Self {
        Self {
            stream: iter.enumerate().peekable(),
        }
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        use TokenKind::*;

        let mut last = self.stream.peek()?.0;
        let Some((loc, byte)) = self.stream.find(|c| {
            last = c.0;
            !c.1.is_ascii_whitespace()
        }) else {
            return Some(Token(Location(last), End));
        };

        Some(match byte {
            b':' => Token(
                Location(loc),
                Bytes({
                    iter::from_fn(|| match (self.stream.next(), self.stream.peek()) {
                        (Some((_, b':')), Some((_, b':'))) => {
                            self.stream.next();
                            Some(b':')
                        }
                        (Some((_, b':')), _) => None,
                        (pair, _) => pair.map(|c| c.1),
                    })
                    .collect()
                }),
            ),

            b',' => Token(Location(loc), Comma),
            b'[' => Token(Location(loc), OpenBracket),
            b']' => Token(Location(loc), CloseBracket),
            b'{' => Token(Location(loc), OpenBrace),
            b'}' => Token(Location(loc), CloseBrace),

            b'#' => {
                self.stream.find(|c| b'\n' == c.1)?;
                return self.next();
            }

            c if c.is_ascii_lowercase() => Token(
                Location(loc),
                Word(
                    String::from_utf8(
                        iter::once(c)
                            .chain(iter::from_fn(|| {
                                self.stream
                                    .next_if(|c| c.1.is_ascii_lowercase())
                                    .map(|c| c.1)
                            }))
                            .collect(),
                    )
                    .unwrap(),
                ),
            ),

            c if c.is_ascii_digit() => Token(
                Location(loc),
                Number({
                    let mut r = 0usize;
                    let (shift, digits) = match (c, self.stream.peek()) {
                        (b'0', Some((_, b'B' | b'b'))) => {
                            self.stream.next();
                            (1, b"01".as_slice())
                        }
                        (b'0', Some((_, b'O' | b'o'))) => {
                            self.stream.next();
                            (3, b"01234567".as_slice())
                        }
                        (b'0', Some((_, b'X' | b'x'))) => {
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
                        .and_then(|c| digits.iter().position(|&k| k == c.1 | 32))
                    {
                        self.stream.next();
                        r = if 0 == shift {
                            r * 10 + k
                        } else {
                            r << shift | k
                        };
                    }
                    if 0 == shift && self.stream.peek().is_some_and(|c| b'.' == c.1) {
                        self.stream.next();
                        let mut d = match self.stream.next() {
                            Some(c) if c.1.is_ascii_digit() => (c.1 - b'0') as usize,
                            _ => return Some(Token(Location(loc), Unknown(r.to_string() + "."))),
                        };
                        let mut w = 10usize;
                        while let Some(c) =
                            self.stream.peek().map(|c| c.1).filter(u8::is_ascii_digit)
                        {
                            self.stream.next();
                            d = d * 10 + (c - b'0') as usize;
                            w *= 10;
                        }
                        r as f64 + (d as f64 / w as f64)
                    } else {
                        r as f64
                    }
                }),
            ),

            c => Token(
                Location(loc),
                Unknown(
                    String::from_utf8_lossy(
                        &iter::once(c)
                            .chain(
                                self.stream
                                    .by_ref()
                                    .map(|c| c.1)
                                    .take_while(|c| !c.is_ascii_whitespace()),
                            )
                            .collect::<Vec<_>>(),
                    )
                    .into_owned(),
                ),
            ),
        })
    }
}
// }}}

// parsing into tree {{{
struct Parser<I: Iterator<Item = u8>> {
    peekable: iter::Peekable<Lexer<I>>,
    types: TypeList,
    errors: ErrorList,
}

impl<I: Iterator<Item = u8>> Parser<I> {
    fn new(iter: I) -> Parser<I> {
        Parser {
            peekable: Lexer::new(iter.into_iter()).peekable(),
            types: TypeList::default(),
            errors: ErrorList::new(),
        }
    }

    fn report(&mut self, err: Error) {
        self.errors.push(err);
    }
    fn report_caused(&mut self, err: Error, loc_ctx: Location, because: ErrorContext) {
        self.errors.push(Error(
            loc_ctx,
            ErrorKind::ContextCaused {
                error: Box::new(err),
                because,
            },
        ));
    }

    fn parse_value(&mut self) -> Tree {
        use TokenKind::*;
        let mkerr = |types: &mut TypeList| {
            TreeKind::Apply(
                types.named(types::ERROR_TYPE),
                UNKNOWN_NAME.to_string(),
                Vec::new(),
            )
        };

        if let Some(Token(loc, kind @ CloseBracket)) | Some(Token(loc, kind @ CloseBrace)) =
            self.peekable.peek()
        {
            let loc = *loc;
            let kind = kind.clone();
            self.report(Error(
                loc,
                ErrorKind::Unexpected {
                    token: kind.clone(),
                    expected: "a value",
                },
            ));
            return Tree(loc, mkerr(&mut self.types));
        }

        let Some(Token(first_loc, first_kind)) = self.peekable.next() else {
            // NOTE: this is the only place where we can get a None out
            // of the lexer iff the input byte stream was exactly empty
            self.report(Error(
                Location(0),
                ErrorKind::Unexpected {
                    token: TokenKind::End,
                    expected: "a value",
                },
            ));
            return Tree(Location(0), mkerr(&mut self.types));
        };

        Tree(
            first_loc,
            match first_kind {
                Word(w) => match lookup_type(&w, &mut self.types) {
                    Some(ty) => TreeKind::Apply(ty, w, Vec::new()),
                    None => {
                        self.report(Error(first_loc, ErrorKind::UnknownName(w)));
                        mkerr(&mut self.types)
                    }
                },

                Bytes(b) => TreeKind::Bytes(b),
                Number(n) => TreeKind::Number(n),

                open_token @ OpenBracket => {
                    let Tree(_, r) = self.parse_script();
                    match self.peekable.peek() {
                        Some(Token(_, CloseBracket)) => _ = self.peekable.next(),
                        Some(Token(here, kind)) => {
                            let err = Error(
                                *here,
                                ErrorKind::Unexpected {
                                    token: kind.clone(),
                                    expected: "next argument or closing ']'",
                                },
                            );
                            if let Comma = kind {
                                self.peekable.next();
                            }
                            self.report_caused(err, first_loc, ErrorContext::Unmatched(open_token))
                        }
                        None => (),
                    }
                    r
                }

                open_token @ OpenBrace => {
                    let mut items = Vec::new();
                    let ty = self.types.named("a");
                    if let Some(Token(_, CloseBrace)) = self.peekable.peek() {
                        self.peekable.next();
                    } else {
                        loop {
                            let item = self.parse_apply();
                            Type::harmonize(ty, item.get_type(), &mut self.types).unwrap_or_else(
                                |e| {
                                    self.report_caused(
                                        Error(item.get_loc(), e),
                                        first_loc,
                                        // FIXME: but this is too late, rigth?
                                        ErrorContext::TypeListInferred(self.types.frozen(ty)),
                                    )
                                },
                            );
                            items.push(item);
                            // (continues only if ',' and then not '}')
                            match self.peekable.next() {
                                Some(Token(_, CloseBrace)) => (),
                                Some(Token(_, Comma)) => {
                                    if let Some(Token(_, CloseBrace)) = self.peekable.peek() {
                                        self.peekable.next();
                                    } else {
                                        continue;
                                    }
                                }
                                Some(Token(here, kind)) => self.report_caused(
                                    Error(
                                        here,
                                        ErrorKind::Unexpected {
                                            token: kind.clone(),
                                            expected: "next item or closing '}'",
                                        },
                                    ),
                                    first_loc,
                                    ErrorContext::Unmatched(open_token),
                                ),
                                None => (),
                            }
                            break;
                        }
                    }
                    TreeKind::List(self.types.list(self.types.finite(), ty), items)
                }

                _ => {
                    self.report(Error(
                        first_loc,
                        ErrorKind::Unexpected {
                            token: first_kind,
                            expected: "a value",
                        },
                    ));
                    mkerr(&mut self.types)
                }
            },
        )
    }

    fn parse_apply(&mut self) -> Tree {
        use TokenKind::*;
        let mut r = self.parse_value();
        while !matches!(
            self.peekable.peek(),
            Some(Token(_, Comma) | Token(_, CloseBracket) | Token(_, CloseBrace) | Token(_, End))
                | None
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
                match func.1 {
                    TreeKind::Apply(_, ref name, args) if COMPOSE_OP_FUNC_NAME == name => {
                        let mut args = args.into_iter();
                        let (f, mut g) = (args.next().unwrap(), args.next().unwrap());
                        // (,)(f, g) => g(f(..))
                        g.try_apply(apply_maybe_unfold(p, f), &mut p.types)
                            .unwrap_or_else(|e| p.report(e));
                        g
                    }
                    _ => {
                        func.try_apply(p.parse_value(), &mut p.types)
                            .unwrap_or_else(|e| p.report(e));
                        func
                    }
                }
            }
            r = apply_maybe_unfold(self, r);
        }
        r
    }

    fn parse_script(&mut self) -> Tree {
        use TokenKind::*;
        let mut r = self.parse_apply();
        if let Some(Token(comma_loc, Comma)) = self.peekable.peek() {
            let mut comma_loc = *comma_loc;
            self.peekable.next();
            let mut then = self.parse_apply();
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            if Type::applicable(then.get_type(), r.get_type(), &self.types) {
                while let Some(Token(_, Comma)) = {
                    then.try_apply(r, &mut self.types)
                        .unwrap_or_else(|e| self.report(e));
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
                // (do..while)
                while let Some(Token(then_comma_loc, Comma)) = {
                    let mut compose = Tree(
                        comma_loc,
                        TreeKind::Apply(
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
                        ),
                    );
                    compose
                        .try_apply(r, &mut self.types)
                        .unwrap_or_else(|e| self.report(e));
                    compose
                        .try_apply(then, &mut self.types)
                        .unwrap_or_else(|e| self.report(e));
                    r = compose;
                    self.peekable.peek()
                } {
                    comma_loc = *then_comma_loc;
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
        let niw = Type::applied(self.get_type(), arg.get_type(), types).map_err(|e| {
            Error(
                // FIXME: type errors accross ',' op are unhelpful
                self.get_loc(),
                ErrorKind::ContextCaused {
                    error: Box::new(Error(arg.get_loc(), e)),
                    because: ErrorContext::AsNthArgTo(
                        if let TreeKind::Apply(_, _, args) = &self.1 {
                            args.len()
                        } else {
                            0
                        },
                        // FIXME: but this is too late, rigth?
                        types.frozen(self.get_type()),
                    ),
                },
            )
        })?;
        let TreeKind::Apply(ty, _, args) = &mut self.1 else {
            unreachable!();
        };
        *ty = niw;
        args.push(arg);
        Ok(())
    }

    fn get_type(&self) -> TypeRef {
        match self.1 {
            TreeKind::Bytes(_) => types::STRFIN_TYPEREF,
            TreeKind::Number(_) => types::NUMBER_TYPEREF,

            TreeKind::List(ty, _) => ty,

            TreeKind::Apply(ty, _, _) => ty,
        }
    }

    fn get_loc(&self) -> Location {
        self.0
    }

    #[allow(dead_code)]
    pub fn new(iter: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        if p.errors.is_empty() {
            Ok(r)
        } else {
            Err(p.errors)
        }
    }

    #[allow(dead_code)]
    pub fn new_typed(iter: impl IntoIterator<Item = u8>) -> Result<(FrozenType, Tree), ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        if p.errors.is_empty() {
            Ok((p.types.frozen(r.get_type()), r))
        } else {
            Err(p.errors)
        }
    }

    /// idk, will surely remove
    #[allow(dead_code)]
    pub fn new_fully_typed(
        iter: impl IntoIterator<Item = u8>,
    ) -> Result<(TypeRef, TypeList, Tree), ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        if p.errors.is_empty() {
            Ok((r.get_type(), p.types, r))
        } else {
            Err(p.errors)
        }
    }
}

impl Display for Tree {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match &self.1 {
            TreeKind::Bytes(v) => write!(f, ":{v:?}:"),
            TreeKind::Number(n) => write!(f, "{n}"),

            TreeKind::List(_, items) => {
                write!(f, "{{")?;
                let mut sep = "";
                for it in items {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, "}}")
            }

            TreeKind::Apply(_, w, empty) if empty.is_empty() => write!(f, "{w}"),
            TreeKind::Apply(_, name, args) => {
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
            panic!()
        })
    }

    fn compare(left: &Tree, right: &Tree) -> bool {
        match (&left.1, &right.1) {
            (TreeKind::List(_, la), TreeKind::List(_, ra)) => la
                .iter()
                .zip(ra.iter())
                .find(|(l, r)| !compare(l, r))
                .is_none(),
            (TreeKind::Apply(_, ln, la), TreeKind::Apply(_, rn, ra))
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
        use TokenKind::*;

        assert_eq!(
            Vec::<Token>::new(),
            Lexer::new("".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[Token(Location(0), Word("coucou".into()))][..],
            Lexer::new("coucou".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token(Location(0), OpenBracket),
                Token(Location(1), Word("a".into())),
                Token(Location(3), Number(1.0)),
                Token(Location(5), Word("b".into())),
                Token(Location(6), Comma),
                Token(Location(8), OpenBrace),
                Token(Location(9), Word("w".into())),
                Token(Location(10), Comma),
                Token(Location(12), Word("x".into())),
                Token(Location(13), Comma),
                Token(Location(15), Word("y".into())),
                Token(Location(16), Comma),
                Token(Location(18), Word("z".into())),
                Token(Location(19), CloseBrace),
                Token(Location(20), Comma),
                Token(Location(22), Number(0.5)),
                Token(Location(25), CloseBracket)
            ][..],
            Lexer::new("[a 1 b, {w, x, y, z}, 0.5]".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token(Location(0), Bytes(vec![104, 97, 121])),
                Token(
                    Location(6),
                    Bytes(vec![104, 101, 121, 58, 32, 110, 111, 116, 32, 104, 97, 121])
                ),
                Token(Location(22), Bytes(vec![])),
                Token(Location(25), Bytes(vec![58])),
                Token(Location(30), Word("fin".into()))
            ][..],
            Lexer::new(":hay: :hey:: not hay: :: :::: fin".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token(Location(0), Number(42.0)),
                Token(Location(3), Bytes(vec![42])),
                Token(Location(7), Number(42.0)),
                Token(Location(12), Number(42.0)),
                Token(Location(21), Number(42.0)),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        use TreeKind::*;

        assert_eq!(
            Err({
                let mut el = ErrorList::new();
                el.push(Error(
                    Location(0),
                    ErrorKind::Unexpected {
                        token: TokenKind::End,
                        expected: "a value",
                    },
                ));
                el
            }),
            Tree::new_typed("".bytes())
        );

        let (ty, res) = expect(Tree::new_typed("input".bytes()));
        assert_tree!(Tree(Location(0), Apply(0, "input".into(), vec![])), res);
        assert_eq!(FrozenType::Bytes(false), ty);

        let (ty, res) = expect(Tree::new_typed(
            "input, split:-:, map[tonum, add1, tostr], join:+:".bytes(),
        ));
        assert_tree!(
            Tree(
                Location(42),
                Apply(
                    0,
                    "join".into(),
                    vec![
                        Tree(Location(46), Bytes(vec![43])),
                        Tree(
                            Location(17),
                            Apply(
                                0,
                                "map".into(),
                                vec![
                                    Tree(
                                        Location(20),
                                        Apply(
                                            0,
                                            COMPOSE_OP_FUNC_NAME.into(),
                                            vec![
                                                Tree(
                                                    Location(26),
                                                    Apply(
                                                        0,
                                                        COMPOSE_OP_FUNC_NAME.into(),
                                                        vec![
                                                            Tree(
                                                                Location(21),
                                                                Apply(0, "tonum".into(), vec![])
                                                            ),
                                                            Tree(
                                                                Location(28),
                                                                Apply(
                                                                    0,
                                                                    "add".into(),
                                                                    vec![Tree(
                                                                        Location(31),
                                                                        Number(1.0)
                                                                    )]
                                                                )
                                                            )
                                                        ]
                                                    )
                                                ),
                                                Tree(
                                                    Location(34),
                                                    Apply(0, "tostr".into(), vec![])
                                                )
                                            ]
                                        )
                                    ),
                                    Tree(
                                        Location(7),
                                        Apply(
                                            0,
                                            "split".into(),
                                            vec![
                                                Tree(Location(12), Bytes(vec![45])),
                                                Tree(Location(0), Apply(0, "input".into(), vec![]))
                                            ]
                                        )
                                    )
                                ]
                            )
                        )
                    ]
                )
            ),
            res
        );
        // FIXME: this should be false, right?
        assert_eq!(FrozenType::Bytes(true), ty);

        let (ty, res) = expect(Tree::new_typed("tonum, add234121, tostr, ln".bytes()));
        assert_tree!(
            Tree(
                Location(23),
                Apply(
                    0,
                    COMPOSE_OP_FUNC_NAME.into(),
                    vec![
                        Tree(
                            Location(16),
                            Apply(
                                0,
                                COMPOSE_OP_FUNC_NAME.into(),
                                vec![
                                    Tree(
                                        Location(5),
                                        Apply(
                                            0,
                                            COMPOSE_OP_FUNC_NAME.into(),
                                            vec![
                                                Tree(Location(0), Apply(0, "tonum".into(), vec![])),
                                                Tree(
                                                    Location(7),
                                                    Apply(
                                                        0,
                                                        "add".into(),
                                                        vec![Tree(Location(10), Number(234121.0))]
                                                    )
                                                )
                                            ]
                                        )
                                    ),
                                    Tree(Location(18), Apply(0, "tostr".into(), vec![]))
                                ]
                            )
                        ),
                        Tree(Location(25), Apply(0, "ln".into(), vec![]))
                    ]
                )
            ),
            res
        );
        assert_eq!(
            FrozenType::Func(
                Box::new(FrozenType::Bytes(false)),
                Box::new(FrozenType::Bytes(true))
            ),
            ty
        );

        let (ty, res) = expect(Tree::new_typed("[tonum, add234121, tostr] :13242:".bytes()));
        assert_tree!(
            Tree(
                Location(19),
                Apply(
                    0,
                    "tostr".into(),
                    vec![Tree(
                        Location(8),
                        Apply(
                            0,
                            "add".into(),
                            vec![
                                Tree(Location(11), Number(234121.0)),
                                Tree(
                                    Location(1),
                                    Apply(
                                        0,
                                        "tonum".into(),
                                        vec![Tree(Location(26), Bytes(vec![49, 51, 50, 52, 50]))]
                                    )
                                )
                            ]
                        )
                    )]
                )
            ),
            res
        );
        assert_eq!(FrozenType::Bytes(true), ty);
    }
}
// }}}
