use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin;
use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::types::{self, FrozenType, Type, TypeList, TypeRef};

pub const COMPOSE_OP_FUNC_NAME: &str = "(,)";

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
    Equal,
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
    Pair(TypeRef, Box<Tree>, Box<Tree>),
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
                b'=' => Equal,

                b'#' => {
                    self.stream.find(|c| b'\n' == c.1)?;
                    return self.next();
                }
                b'_' => Word("_".into()), // ..keeping this case for those that are used to it

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
                    String::from_utf8_lossy({
                        let chclass = c.is_ascii_alphanumeric();
                        &iter::once(c)
                            .chain(iter::from_fn(|| {
                                self.stream
                                    .next_if(|c| {
                                        c.1.is_ascii_alphanumeric() == chclass
                                            && !c.1.is_ascii_whitespace()
                                    })
                                    .map(|c| c.1)
                            }))
                            .collect::<Vec<_>>()
                    })
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
}

fn err_context_complete_type(types: &TypeList, err: ErrorKind, it: &Tree) -> ErrorKind {
    let complete_type = types.frozen(it.get_type());
    match &err {
        ErrorKind::ExpectedButGot {
            expected: _,
            actual,
        } if complete_type != *actual => ErrorKind::ContextCaused {
            error: Box::new(Error(it.get_loc(), err)),
            because: ErrorContext::CompleteType { complete_type },
        },
        _ => err,
    }
}

fn err_context_auto_coerced(
    arg_loc: Location,
    err: ErrorKind,
    func_name: String,
    func_type: FrozenType,
) -> ErrorKind {
    ErrorKind::ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: ErrorContext::AutoCoercedVia {
            func_name,
            func_type,
        },
    }
}

fn err_context_as_nth_arg(
    types: &TypeList,
    arg_loc: Location,
    err: ErrorKind,
    comma_loc: Option<Location>, // if some, ChainedFrom..
    func: &Tree,
) -> ErrorKind {
    let type_with_curr_args = types.frozen(func.get_type());
    ErrorKind::ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: {
            // unwrap: see `context_chained_as_nth` call sites
            let (nth_arg, func_name) = func.get_nth_arg_func_name().unwrap();
            match comma_loc {
                Some(comma_loc) => ErrorContext::ChainedFromAsNthArgToNamedNowTyped {
                    comma_loc,
                    nth_arg,
                    func_name,
                    type_with_curr_args,
                },
                None => ErrorContext::AsNthArgToNamedNowTyped {
                    nth_arg,
                    func_name,
                    type_with_curr_args,
                },
            }
        },
    }
}

fn err_not_func(types: &TypeList, func: &Tree) -> ErrorKind {
    match func.get_nth_arg_func_name() {
        Some((nth_arg, func_name)) => ErrorKind::TooManyArgs { nth_arg, func_name },
        None => ErrorKind::NotFunc {
            actual_type: types.frozen(func.get_type()),
        },
    }
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

    fn mktypeof(&mut self, name: &str) -> (TreeKind, TypeRef) {
        let ty = self.types.named(&format!("typeof({name})"));
        (TreeKind::Apply(ty, name.into(), Vec::new()), ty)
    }

    fn parse_value(&mut self) -> Tree {
        use TokenKind::*;

        if let Some(Token(loc, kind @ (Comma | CloseBracket | CloseBrace | End))) =
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
            return Tree(loc, self.mktypeof("").0);
        }

        let Some(Token(first_loc, first_kind)) = self.peekable.next() else {
            // this is the only place where we can get a None out of the lexer
            // iif the input byte stream was exactly empty
            self.report(Error(
                Location(0),
                ErrorKind::Unexpected {
                    token: TokenKind::End,
                    expected: "a value",
                },
            ));
            // anything, doesn't mater what, this is the easiest..
            return Tree(Location(0), TreeKind::Number(0.0));
        };

        let fst = Tree(
            first_loc.clone(),
            match first_kind {
                Word(w) => match builtin::NAMES.get(&w) {
                    Some((mkty, _)) => TreeKind::Apply(mkty(&mut self.types), w, Vec::new()),
                    None => {
                        let (r, ty) = self.mktypeof(&w);
                        self.report(Error(
                            first_loc,
                            ErrorKind::UnknownName {
                                name: w,
                                expected_type: unsafe {
                                    // NOTE: this is needlessly quirky, stores the usize in place of
                                    //       the FrozenType (works because usize is smaller); see at
                                    //       the receiving end (in parse) where the error list is
                                    //       iterated through once to then compute the correct
                                    //       FrozenType (it cannot be done here and now because it is
                                    //       not fully known yet..)
                                    //       also relies on the fact that (prays that) rust will not do
                                    //       anything with this invalid data, and that we'll have
                                    //       a chance to fix it up before it goes out into the user
                                    //       world
                                    let mut fake_frozen = std::mem::MaybeUninit::zeroed();
                                    *(fake_frozen.as_mut_ptr() as *mut usize) = ty;
                                    fake_frozen.assume_init()
                                },
                            },
                        ));
                        r
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
                                    because: ErrorContext::Unmatched { open_token },
                                },
                            ));
                        }
                        None => (),
                    }
                    r
                }

                open_token @ OpenBrace => {
                    let mut items = Vec::new();
                    let ty = self.types.named("itemof(typeof({}))");
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
                                        error: Box::new(Error(
                                            item.get_loc(),
                                            err_context_complete_type(&self.types, e, &item),
                                        )),
                                        because: ErrorContext::TypeListInferredItemType {
                                            list_item_type: snapshot.frozen(ty),
                                        },
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
                                        because: ErrorContext::Unmatched { open_token },
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

                Comma | CloseBracket | CloseBrace | End => unreachable!(),

                Equal => {
                    self.report(Error(
                        first_loc,
                        ErrorKind::Unexpected {
                            token: first_kind,
                            expected: "a value",
                        },
                    ));
                    self.mktypeof("=").0
                }

                Unknown(ref w) => {
                    let r = self.mktypeof(w).0;
                    self.report(Error(
                        first_loc,
                        ErrorKind::Unexpected {
                            token: first_kind,
                            expected: "a value",
                        },
                    ));
                    r
                }
            },
        );

        if let Some(Token(equal_loc, Equal)) = self.peekable.peek() {
            let equal_loc = equal_loc.clone();
            self.peekable.next();
            let snd = self.parse_value();
            let ty = self.types.pair(fst.get_type(), snd.get_type());
            Tree(equal_loc, TreeKind::Pair(ty, Box::new(fst), Box::new(snd)))
        } else {
            fst
        }
    }

    fn parse_apply(&mut self) -> Tree {
        use TokenKind::*;
        let mut r = self.parse_value();
        while !matches!(
            self.peekable.peek(),
            Some(Token(_, Comma | CloseBracket | CloseBrace | End)) | None
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
                        // unwrap because cannot fail:
                        //     with f :: a -> b and g :: b -> c
                        //     apply_maybe_unfold(p, f) :: b
                        g.try_apply(apply_maybe_unfold(p, f), &mut p.types).unwrap();
                        g
                    }
                    TreeKind::Apply(ty, _, _)
                        if matches!(p.types.get(ty), Type::Func(_, _) | Type::Named(_)) =>
                    {
                        // XXX: does it need `snapshot` here?
                        func.try_apply(p.parse_value(), &mut p.types)
                            .unwrap_or_else(|(e, arg)| {
                                // `func` is a function (matches ::Apply)
                                let e =
                                    err_context_as_nth_arg(&p.types, arg.get_loc(), e, None, &func);
                                p.report(Error(func.get_loc(), e));
                                func.force_apply(&mut p.types);
                            });
                        func
                    }
                    _ => {
                        drop(p.parse_value());
                        let e = err_not_func(&p.types, &func);
                        p.report(Error(func.get_loc(), e));
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
        while let Some(Token(comma_loc, _)) = self.peekable.next_if(|t| TokenKind::Comma == t.1) {
            let mut then = self.parse_apply();

            let r_ty = r.get_type();
            let r_tty = self.types.get(r_ty);
            let r_loc = r.get_loc();

            let then_ty = then.get_type();
            let then_tty = self.types.get(then_ty);
            let then_loc = then.get_loc();

            let (is_func, is_hole) = match then_tty {
                Type::Func(_, _) => (true, false),
                Type::Named(_) => (false, true),
                _ => (false, false),
            };

            // `then` not a function nor a type hole
            r = if !is_func && !is_hole {
                self.report(Error(
                    r_loc,
                    ErrorKind::ContextCaused {
                        error: Box::new(Error(
                            then_loc,
                            match then.get_nth_arg_func_name() {
                                Some((nth_arg, func_name)) => {
                                    ErrorKind::TooManyArgs { nth_arg, func_name }
                                }
                                None => ErrorKind::NotFunc {
                                    actual_type: self.types.frozen(then_ty),
                                },
                            },
                        )),
                        because: ErrorContext::ChainedFromToNotFunc { comma_loc },
                    },
                ));
                then
            }
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            else if is_hole
                || !matches!(r_tty, Type::Func(_, _)) // NOTE: catches coersion cases
                || Type::applicable(then_ty, r_ty, &self.types)
            {
                // XXX: does it need `snapshot` here?
                then.try_apply(r, &mut self.types).unwrap_or_else(|(e, r)| {
                    // we know `then` is a function (if it was hole then `try_apply` mutates it)
                    let e =
                        err_context_as_nth_arg(&self.types, r.get_loc(), e, Some(comma_loc), &then);
                    self.report(Error(then.get_loc(), e));
                    then.force_apply(&mut self.types);
                });
                then
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                // XXX: does it need `snapshot` here?
                let mut compose = Tree(
                    comma_loc.clone(),
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
                // unwrap: we know that `r` is a function (see previous 'else if')
                compose.try_apply(r, &mut self.types).unwrap();
                compose
                    .try_apply(then, &mut self.types)
                    .unwrap_or_else(|(e, then)| {
                        // `then` is either a function or a type hole ('if')
                        // and it is not a type hole ('else if'), so it is a function
                        let e =
                            err_context_as_nth_arg(&self.types, r_loc, e, Some(comma_loc), &then);
                        self.report(Error(then.get_loc(), e));
                        // TODO: eg. `tostr, add1, _`
                        //       this generates unhelpful typing when `try_apply` failed, even
                        //       after a `force_apply` the type is still (x -> y) where y is the
                        //       one we would want to show
                        compose.force_apply(&mut self.types);
                    });
                compose
            };
        } // while let Comma
        r
    }

    fn parse(mut self) -> Result<(FrozenType, Tree), ErrorList> {
        let r = self.parse_script();
        if self.errors.is_empty() {
            Ok((self.types.frozen(r.get_type()), r))
        } else {
            // NOTE: this is the receiving end from `parse_value` when a name was not found: this
            //       is the point where we get the usizes again and compute the final FrozenType
            for e in self.errors.0.iter_mut() {
                if let ErrorKind::UnknownName {
                    name: _,
                    expected_type: frty,
                } = &mut e.1
                {
                    let really_usize = frty as *const FrozenType as *const usize;
                    let ty = unsafe { *really_usize };
                    std::mem::forget(std::mem::replace(frty, self.types.frozen(ty)));
                }
            }
            Err(self.errors)
        }
    }
}
// }}}

// uuhh.. tree itself (the entry point, Tree::new) {{{
impl Tree {
    fn try_apply(&mut self, mut arg: Tree, types: &mut TypeList) -> Result<(), (ErrorKind, Tree)> {
        use Type::*;

        let TreeKind::Apply(func_ty, ref name, args) = &mut self.1 else {
            unreachable!();
        };
        let is_compose = COMPOSE_OP_FUNC_NAME == name;

        if let Named(name) = types.get(*func_ty) {
            let name = name.clone();
            let par = types.named(&format!("paramof({name})"));
            let ret = types.named(&format!("returnof({name})"));
            *types.get_mut(*func_ty) = Func(par, ret);
        }

        // coerce if needed
        let Func(par_ty, _) = types.get(*func_ty) else {
            unreachable!();
        };
        let coerce = if let Some(w) = match (types.get(*par_ty), types.get(arg.get_type())) {
            (Number, Bytes(_)) => Some("tonum"),
            (Bytes(_), Number) => Some("tostr"),
            (List(_, num), Bytes(_)) if matches!(types.get(*num), Number) => Some("codepoints"),
            (List(_, any), Bytes(_)) if matches!(types.get(*any), Bytes(_) | Named(_)) => {
                Some("graphemes")
            }
            (Bytes(_), List(_, num)) if matches!(types.get(*num), Number) => Some("uncodepoints"),
            (Bytes(_), List(_, bst)) if matches!(types.get(*bst), Bytes(_)) => Some("ungraphemes"),
            _ => None,
        } {
            let (mkty, _) = builtin::NAMES.get(w).unwrap();
            let mut func = Tree(
                arg.0.clone(),
                TreeKind::Apply(mkty(types), w.into(), Vec::new()),
            );
            let fty = types.frozen(func.get_type());
            func.try_apply(arg, types).unwrap();
            arg = func;
            Some((arg.0.clone(), w, fty))
        } else {
            None
        };

        match Type::applied(*func_ty, arg.get_type(), types) {
            Ok(ret_ty) => {
                *func_ty = ret_ty;
                args.push(arg);
                Ok(())
            }
            Err(e) => Err((
                {
                    let mut e = err_context_complete_type(
                        types,
                        e,
                        if is_compose { &args[0] } else { &arg },
                    );
                    if let Some((loc, name, fty)) = coerce {
                        e = err_context_auto_coerced(loc, e, name.into(), fty);
                    }
                    e
                },
                arg,
            )),
        }
    }

    fn force_apply(&mut self, types: &mut TypeList) {
        let TreeKind::Apply(func_ty, _, args) = &mut self.1 else {
            unreachable!();
        };
        let ret_ty = match types.get(*func_ty) {
            &Type::Func(_, ret) => ret,
            _ => unreachable!(),
        };

        *func_ty = ret_ty;
        // anything, doesn't mater what, this is the easiest..
        args.push(Tree(Location(0), TreeKind::Number(0.0)));
    }

    fn get_type(&self) -> TypeRef {
        use TreeKind::*;
        match self.1 {
            Bytes(_) => types::STRFIN_TYPEREF,
            Number(_) => types::NUMBER_TYPEREF,
            List(ty, _) => ty,
            Apply(ty, _, _) => ty,
            Pair(ty, _, _) => ty,
        }
    }

    fn get_loc(&self) -> Location {
        self.0.clone()
    }

    fn get_nth_arg_func_name(&self) -> Option<(usize, String)> {
        match &self.1 {
            TreeKind::Apply(_, name, args) => Some((args.len() + 1, name.clone())),
            _ => None,
        }
    }

    #[allow(dead_code)]
    pub fn new(iter: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        Parser::new(iter.into_iter()).parse().map(|r| r.1)
    }

    #[allow(dead_code)]
    pub fn new_typed(iter: impl IntoIterator<Item = u8>) -> Result<(FrozenType, Tree), ErrorList> {
        Parser::new(iter.into_iter()).parse()
    }
}

impl Display for Tree {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        use TreeKind::*;
        match &self.1 {
            Bytes(v) => write!(f, ":{v:?}:"),
            Number(n) => write!(f, "{n}"),

            List(_, items) => {
                write!(f, "{{")?;
                let mut sep = "";
                for it in items {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, "}}")
            }

            Apply(_, w, empty) if empty.is_empty() => write!(f, "{w}"),
            Apply(_, name, args) => {
                write!(f, "{name}(")?;
                let mut sep = "";
                for it in args {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, ")")
            }

            Pair(_, fst, snd) => write!(f, "({fst}, {snd})"),
        }
    }
}
// }}}
