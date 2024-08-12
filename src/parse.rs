use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin::NAMES;
use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::types::{self, FrozenType, Type, TypeList, TypeRef};

pub const COMPOSE_OP_FUNC_NAME: &str = "(,)";
pub const TYPEHOLE_OBJECT_NAME: &str = "_";
const UNKNOWN_NAME: &str = "{unknown}";

#[derive(PartialEq, Debug, Clone)]
pub struct Location(pub(crate) usize);

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

#[derive(PartialEq, Debug)]
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
pub(crate) struct Lexer<I: Iterator<Item = u8>> {
    stream: iter::Peekable<iter::Enumerate<I>>,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    pub(crate) fn new(iter: I) -> Self {
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

        Some(Token(
            Location(loc),
            match byte {
                b':' => Bytes({
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

                b',' => Comma,
                b'[' => OpenBracket,
                b']' => CloseBracket,
                b'{' => OpenBrace,
                b'}' => CloseBrace,

                b'#' => {
                    self.stream.find(|c| b'\n' == c.1)?;
                    return self.next();
                }
                b'_' => Word(TYPEHOLE_OBJECT_NAME.into()),

                c if c.is_ascii_lowercase() => Word(
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

                c if c.is_ascii_digit() => Number({
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

                c => Unknown(
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
            },
        ))
    }
}
// }}}

// parsing into tree {{{
struct Parser<I: Iterator<Item = u8>> {
    peekable: iter::Peekable<Lexer<I>>,
    types: TypeList,
    errors: ErrorList,
    holes: Vec<(Location, TypeRef)>,
}

impl<I: Iterator<Item = u8>> Parser<I> {
    fn new(iter: I) -> Parser<I> {
        Parser {
            peekable: Lexer::new(iter.into_iter()).peekable(),
            types: TypeList::default(),
            errors: ErrorList::new(),
            holes: Vec::new(),
        }
    }

    fn report(&mut self, err: Error) {
        self.errors.push(err);
    }

    fn parse_value(&mut self) -> Tree {
        use TokenKind::*;
        let mkerr = || TreeKind::Apply(types::ERROR_TYPEREF, UNKNOWN_NAME.to_string(), Vec::new());

        if let Some(Token(loc, kind @ CloseBracket)) | Some(Token(loc, kind @ CloseBrace)) =
            self.peekable.peek()
        {
            let loc = loc.clone();
            let kind = kind.clone();
            self.report(Error(
                loc.clone(),
                ErrorKind::Unexpected {
                    token: kind,
                    expected: "a value",
                },
            ));
            return Tree(loc, mkerr());
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
            return Tree(Location(0), mkerr());
        };

        Tree(
            first_loc.clone(),
            match first_kind {
                Word(w) => {
                    if TYPEHOLE_OBJECT_NAME == w {
                        let ty = self.types.named("any");
                        self.holes.push((first_loc, ty));
                        TreeKind::Apply(ty, w, Vec::new())
                    } else {
                        match NAMES.get(&w) {
                            Some((mkty, _)) => {
                                TreeKind::Apply(mkty(&mut self.types), w, Vec::new())
                            }
                            None => {
                                self.report(Error(first_loc, ErrorKind::UnknownName(w)));
                                mkerr()
                            }
                        }
                    }
                }

                Bytes(b) => TreeKind::Bytes(b),
                Number(n) => TreeKind::Number(n),

                open_token @ OpenBracket => {
                    let Tree(_, r) = self.parse_script();
                    match self.peekable.peek() {
                        Some(Token(_, CloseBracket)) => _ = self.peekable.next(),
                        Some(Token(here, kind)) => {
                            let err = Error(
                                here.clone(),
                                ErrorKind::Unexpected {
                                    token: kind.clone(),
                                    expected: "next argument or closing ']'",
                                },
                            );
                            if let Comma = kind {
                                self.peekable.next();
                            }
                            self.report(Error(
                                first_loc,
                                ErrorKind::ContextCaused {
                                    error: Box::new(err),
                                    because: ErrorContext::Unmatched(open_token),
                                },
                            ))
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
                            let item_ty = item.get_type();
                            // NOTE: easy way out, used to report type error,
                            // needless cloning for 90% of the time tho
                            let snapshot = self.types.clone();
                            Type::harmonize(ty, item_ty, &mut self.types).unwrap_or_else(|e| {
                                self.report(Error(
                                    first_loc.clone(),
                                    ErrorKind::ContextCaused {
                                        error: Box::new(Error(item.get_loc(), e)),
                                        because: ErrorContext::TypeListInferredItemTypeButItWas(
                                            snapshot.frozen(ty),
                                            snapshot.frozen(item_ty),
                                        ),
                                    },
                                ))
                            });
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
                                Some(Token(here, kind)) => self.report(Error(
                                    first_loc,
                                    ErrorKind::ContextCaused {
                                        error: Box::new(Error(
                                            here,
                                            ErrorKind::Unexpected {
                                                token: kind.clone(),
                                                expected: "next item or closing '}'",
                                            },
                                        )),
                                        because: ErrorContext::Unmatched(open_token),
                                    },
                                )),
                                None => (),
                            }
                            break;
                        }
                    }
                    let fin = self.types.finite(true);
                    TreeKind::List(self.types.list(fin, ty), items)
                }

                _ => {
                    self.report(Error(
                        first_loc,
                        ErrorKind::Unexpected {
                            token: first_kind,
                            expected: "a value",
                        },
                    ));
                    mkerr()
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
                        g.try_apply(apply_maybe_unfold(p, f), &mut p.types, &mut p.holes)
                            .unwrap_or_else(|e| p.report(e));
                        g
                    }
                    _ => {
                        func.try_apply(p.parse_value(), &mut p.types, &mut p.holes)
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
            let mut comma_loc = comma_loc.clone();
            self.peekable.next();
            let mut then = self.parse_apply();

            let (is_func, is_hole) = match &then {
                Tree(_, TreeKind::Apply(ty, name, _)) => (
                    matches!(self.types.get(*ty), Type::Func(_, _)),
                    TYPEHOLE_OBJECT_NAME == name,
                ),
                _ => (false, false),
            };

            // `then` not a function nor a type hole
            if !is_func && !is_hole {
                if types::ERROR_TYPEREF != then.get_type() {
                    self.report(Error(
                        then.get_loc(),
                        ErrorKind::NotFunc(self.types.frozen(then.get_type())),
                    ));
                }
                r.1 = TreeKind::Apply(types::ERROR_TYPEREF, UNKNOWN_NAME.to_string(), Vec::new());
            }
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            else if is_hole || Type::applicable(then.get_type(), r.get_type(), &self.types) {
                while let Some(Token(_, Comma)) = {
                    then.try_apply(r, &mut self.types, &mut self.holes)
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
            else if matches!(self.types.get(r.get_type()), Type::Func(_, _)) {
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
                    // unwrap because `r` already known to be a function
                    compose
                        .try_apply(r, &mut self.types, &mut self.holes)
                        .unwrap();
                    compose
                        .try_apply(then, &mut self.types, &mut self.holes)
                        .unwrap_or_else(|e| self.report(e));
                    r = compose;
                    self.peekable.peek()
                } {
                    comma_loc = then_comma_loc.clone();
                    self.peekable.next();
                    then = self.parse_apply();
                }
            }
            // `r, then` but `r` is neither the arg to `then` nor a function
            // (but we know that `then` is a function from the first 'if')
            else {
                let Tree(then_loc, TreeKind::Apply(ty, name, args)) = then else {
                    unreachable!();
                };
                let ty = self.types.frozen(ty);
                self.report(Error(
                    then_loc,
                    ErrorKind::ContextCaused {
                        error: Box::new(Error(
                            r.get_loc(),
                            ErrorKind::ExpectedButGot(
                                match &ty {
                                    FrozenType::Func(par, _) => *par.clone(),
                                    _ => unreachable!(),
                                },
                                self.types.frozen(r.get_type()),
                            ),
                        )),
                        because: ErrorContext::ChainedFromAsNthArgToNamedNowTyped(
                            comma_loc,
                            args.len() + 1,
                            name,
                            ty,
                        ),
                    },
                ));
            }
        }
        r
    }
}
// }}}

// uuhh.. tree itself (the entry point, Tree::new) {{{
impl Tree {
    fn try_apply(
        &mut self,
        arg: Tree,
        types: &mut TypeList,
        holes: &mut [(Location, TypeRef)],
    ) -> Result<(), Error> {
        // NOTE: easy way out, used to report type error,
        // needless cloning for 90% of the time tho
        let snapshot = types.clone();

        let Tree(base_loc, TreeKind::Apply(func_ty, name, args)) = self else {
            unreachable!();
        };

        if TYPEHOLE_OBJECT_NAME == name {
            let ret = types.named("ret");
            let hole_ty = &mut holes.iter_mut().find(|t| t.1 == *func_ty).unwrap().1;
            *func_ty = types.func(*func_ty, ret);
            *hole_ty = *func_ty;
        }

        match Type::applied(*func_ty, arg.get_type(), types) {
            Ok(ret_ty) => {
                *func_ty = ret_ty;
                args.push(arg);
                Ok(())
            }

            Err(e) => {
                Err(if COMPOSE_OP_FUNC_NAME == name {
                    let (f, g) = (&args[0], &arg);
                    // `then` (ie 'g') is known to be a function from back in `parse_script`
                    // (because had it been a hole, it would have fallen in the '[x, f..]' case)
                    let Tree(then_loc, TreeKind::Apply(ty, name, args)) = g else {
                        unreachable!();
                    };
                    Error(
                        then_loc.clone(),
                        ErrorKind::ContextCaused {
                            error: Box::new(Error(f.get_loc(), e)),
                            because: ErrorContext::ChainedFromAsNthArgToNamedNowTyped(
                                base_loc.clone(), // the (,)
                                args.len() + 1,
                                name.clone(),
                                snapshot.frozen(*ty),
                            ),
                        },
                    )
                } else {
                    Error(
                        base_loc.clone(),
                        ErrorKind::ContextCaused {
                            error: Box::new(Error(arg.get_loc(), e)),
                            because: ErrorContext::AsNthArgToNamedNowTyped(
                                args.len() + 1,
                                name.clone(),
                                snapshot.frozen(self.get_type()),
                            ),
                        },
                    )
                })
            }
        }
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
        self.0.clone()
    }

    #[allow(dead_code)]
    pub fn new(iter: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        let mut p = Parser::new(iter.into_iter());
        let r = p.parse_script();
        for (loc, ty) in p.holes {
            p.errors
                .push(Error(loc, ErrorKind::FoundTypeHole(p.types.frozen(ty))));
        }
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
        for (loc, ty) in p.holes {
            p.errors
                .push(Error(loc, ErrorKind::FoundTypeHole(p.types.frozen(ty))));
        }
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
