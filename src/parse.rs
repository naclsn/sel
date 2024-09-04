use std::iter::{self, Enumerate, Peekable};
use std::mem::{self, MaybeUninit};

use crate::error::{Error, ErrorContext, ErrorKind, ErrorList, Location, SourceRef};
use crate::scope::{Global, Scope, ScopeItem};
use crate::types::{FrozenType, Type, TypeList, TypeRef};

pub struct Processed {
    pub errors: ErrorList,
    pub scope: Scope,
    pub tree: Option<Tree>,
}

pub fn process(source: SourceRef, global: &mut Global) -> Processed {
    let bytes = global.registry.take_bytes(source);
    let p = Parser::new(source, global, bytes.iter().copied());
    let r = p.parse();
    global.registry.put_back_bytes(source, bytes);
    r
}

// other types {{{
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
    Def,
    Let,
    Use,
    Semicolon,
    End,
}

macro_rules! TermToken {
    () => {
        Comma | CloseBracket | CloseBrace | Equal | Semicolon | End
    };
}

#[derive(PartialEq, Debug, Clone)]
pub struct Token(pub Location, pub TokenKind);

#[derive(PartialEq, Debug, Clone)]
pub enum Applicable {
    Name(String),
    Bind(Pattern, Box<Tree>, Option<Box<Tree>>),
}

#[derive(PartialEq, Debug, Clone)]
pub enum TreeKind {
    Bytes(Vec<u8>),
    Number(f64),
    List(Vec<Tree>),
    Apply(Applicable, Vec<Tree>),
    Pair(Box<Tree>, Box<Tree>),
}

#[derive(PartialEq, Debug, Clone)]
pub struct Tree {
    pub loc: Location,
    pub ty: TypeRef,
    pub value: TreeKind,
}

#[derive(PartialEq, Debug, Clone)]
pub enum Pattern {
    Bytes(Vec<u8>),
    Number(f64),
    List(Vec<Pattern>, Option<(Location, String)>),
    Name(Location, String),
    Pair(Box<Pattern>, Box<Pattern>),
}
// }}}

// lexing into tokens {{{
/// note: this is an infinite iterator (`next()` is never `None`)
pub(crate) struct Lexer<I: Iterator<Item = u8>> {
    stream: Peekable<Enumerate<I>>,
    source: SourceRef,
    last_at: usize,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    pub(crate) fn new(source: SourceRef, iter: I) -> Self {
        Self {
            stream: iter.enumerate().peekable(),
            source,
            last_at: 0,
        }
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        use TokenKind::*;

        let Some((at, byte)) = self.stream.find(|c| !c.1.is_ascii_whitespace()) else {
            let range = self.last_at..self.last_at;
            return Some(Token(Location(self.source, range), End));
        };

        let (len, tok) = match byte {
            b':' => {
                let mut doubles = 2;
                let b: Vec<u8> = iter::from_fn(|| match (self.stream.next(), self.stream.peek()) {
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
            b';' => (1, Semicolon),

            b'#' => {
                self.stream.find(|c| {
                    self.last_at = c.0;
                    b'\n' == c.1
                })?;
                return self.next();
            }
            b'_' => (1, Word("_".into())),

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
                        _ => Word(String::from_utf8(b).unwrap()),
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
                        r << shift | k
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
                            return Some(Token(Location(self.source, at..at + l), Unknown(t)));
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
                let chclass = c.is_ascii_alphanumeric();
                let b: Vec<u8> = iter::once(c)
                    .chain(iter::from_fn(|| {
                        self.stream
                            .next_if(|c| {
                                c.1.is_ascii_alphanumeric() == chclass && !c.1.is_ascii_whitespace()
                            })
                            .map(|c| c.1)
                    }))
                    .collect();
                (b.len(), Unknown(String::from_utf8_lossy(&b).into_owned()))
            }
        };

        self.last_at = at + len;
        Some(Token(Location(self.source, at..at + len), tok))
    }
}
// }}}

// parsing into tree {{{
pub struct Parser<'a, I: Iterator<Item = u8>> {
    peekable: Peekable<Lexer<I>>,
    source: SourceRef,
    global: &'a mut Global,
    result: Processed,
}

// error reportig helpers {{{
fn err_context_complete_type(
    types: &TypeList,
    loc: Location,
    err: ErrorKind,
    ty: TypeRef,
) -> ErrorKind {
    let complete_type = types.frozen(ty);
    match &err {
        ErrorKind::ExpectedButGot {
            expected: _,
            actual,
        } if complete_type != *actual => ErrorKind::ContextCaused {
            error: Box::new(Error(loc, err)),
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
    let type_with_curr_args = types.frozen(func.ty);
    ErrorKind::ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: {
            // unreachable: func is a function, see `err_context_as_nth_arg` call sites
            let (nth_arg, func) = match &func.value {
                TreeKind::Apply(app, args) => (args.len() + 1, app.clone()),
                _ => unreachable!(),
            };
            match comma_loc {
                Some(comma_loc) => ErrorContext::ChainedFromAsNthArgToNowTyped {
                    comma_loc,
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
                None => ErrorContext::AsNthArgToNowTyped {
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
            }
        },
    }
}

fn err_not_func(types: &TypeList, func: &Tree) -> ErrorKind {
    match &func.value {
        TreeKind::Apply(name, args) => ErrorKind::TooManyArgs {
            nth_arg: args.len() + 1,
            func: name.clone(),
        },
        _ => ErrorKind::NotFunc {
            actual_type: types.frozen(func.ty),
        },
    }
}

fn err_unexpected(token: &Token, expected: &'static str, unmatched: Option<&Token>) -> Error {
    let Token(here, token) = token.clone();
    let mut err = Error(here, ErrorKind::Unexpected { token, expected });
    if let Some(unmatched) = unmatched {
        let Token(from, open_token) = unmatched.clone();
        err = Error(
            from,
            ErrorKind::ContextCaused {
                error: Box::new(err),
                because: ErrorContext::Unmatched { open_token },
            },
        );
    }
    err
}

fn err_list_type_mismatch(
    types: &TypeList,
    item_loc: Location,
    err: ErrorKind,
    item_type: TypeRef,
    open_loc: Location,
    list_item_type: TypeRef,
) -> Error {
    Error(
        open_loc,
        ErrorKind::ContextCaused {
            error: Box::new(Error(
                item_loc.clone(),
                err_context_complete_type(types, item_loc, err, item_type),
            )),
            because: ErrorContext::TypeListInferredItemType {
                list_item_type: types.frozen(list_item_type),
            },
        },
    )
}

fn err_already_declared(
    types: &TypeList,
    name: String,
    new_loc: Location,
    previous: &ScopeItem,
) -> Error {
    Error(
        previous
            .get_loc()
            .cloned()
            .unwrap_or_else(|| new_loc.clone()),
        ErrorKind::ContextCaused {
            error: Box::new(Error(new_loc, ErrorKind::NameAlreadyDeclared { name })),
            because: ErrorContext::DeclaredHereWithType {
                with_type: match previous {
                    ScopeItem::Builtin(mkty, _) => {
                        let mut types = TypeList::default();
                        let ty = mkty(&mut types);
                        types.frozen(ty)
                    }
                    ScopeItem::Defined(val, _) => types.frozen(val.ty),
                    ScopeItem::Binding(_, ty) => types.frozen(*ty),
                },
            },
        },
    )
}
// }}}

impl<I: Iterator<Item = u8>> Parser<'_, I> {
    pub fn new(source: SourceRef, global: &mut Global, bytes: I) -> Parser<I> {
        let scope = Scope::new(Some(&global.scope));
        Parser {
            peekable: Lexer::new(source, bytes.into_iter()).peekable(),
            source,
            global,
            result: Processed {
                errors: ErrorList::new(),
                scope,
                tree: None,
            },
        }
    }

    fn peek_tok(&mut self) -> &Token {
        self.peekable.peek().unwrap()
    }
    fn next_tok(&mut self) -> Token {
        self.peekable.next().unwrap()
    }
    fn skip_tok(&mut self) {
        self.peekable.next();
    }

    fn report(&mut self, err: Error) {
        self.result.errors.push(err);
    }

    fn mktypeof(&mut self, name: &str) -> (TypeRef, TreeKind) {
        let ty = self.global.types.named(format!("typeof({name})"));
        (
            ty,
            TreeKind::Apply(Applicable::Name(name.into()), Vec::new()),
        )
    }

    /// the following names can be looked up when parsing:
    /// - `pipe`
    /// - `tonum`
    /// - `tostr`
    /// - `codepoints`
    /// - `graphemes`
    /// - `uncodepoints`
    /// - `ungraphemes`
    fn sure_lookup(&mut self, loc: Location, name: &str) -> Tree {
        Tree {
            loc,
            ty: self
                .result
                .scope
                .lookup(name)
                .unwrap()
                .make_type(&mut self.global.types),
            value: TreeKind::Apply(Applicable::Name(name.into()), Vec::new()),
        }
    }

    /// - named to func conversion
    /// - any needed coercion
    /// - if couldn't then still `force_apply`s and err appropriately
    fn try_apply(&mut self, mut func: Tree, mut arg: Tree, comma_loc: Option<Location>) -> Tree {
        use Type::*;

        let TreeKind::Apply(ref base, args) = &mut func.value else {
            unreachable!();
        };
        let is_compose = matches!(base, Applicable::Name(name) if "pipe" == name);

        if let Named(name) = self.global.types.get(func.ty) {
            let name = name.clone();
            let par = self.global.types.named(format!("paramof({name})"));
            let ret = self.global.types.named(format!("returnof({name})"));
            *self.global.types.get_mut(func.ty) = Func(par, ret);
        }

        // coerce if needed
        let Func(par_ty, _) = self.global.types.get(func.ty) else {
            unreachable!();
        };
        let coerce = if let Some(w) = match (
            self.global.types.get(*par_ty),
            self.global.types.get(arg.ty),
        ) {
            (Number, Bytes(_)) => Some("tonum"),
            (Bytes(_), Number) => Some("tostr"),
            (List(_, num), Bytes(_)) if matches!(self.global.types.get(*num), Number) => {
                Some("codepoints")
            }
            (List(_, any), Bytes(_))
                if matches!(self.global.types.get(*any), Bytes(_) | Named(_)) =>
            {
                Some("graphemes")
            }
            (Bytes(_), List(_, num)) if matches!(self.global.types.get(*num), Number) => {
                Some("uncodepoints")
            }
            (Bytes(_), List(_, bst)) if matches!(self.global.types.get(*bst), Bytes(_)) => {
                Some("ungraphemes")
            }
            _ => None,
        } {
            let coerce = self.sure_lookup(arg.loc.clone(), w);
            let fty = self.global.types.frozen(coerce.ty);
            arg = self.try_apply(coerce, arg, None);
            Some((arg.loc.clone(), w, fty))
        } else {
            None
        };

        match Type::applied(func.ty, arg.ty, &mut self.global.types) {
            Ok(ret_ty) => {
                func.ty = ret_ty;
                args.push(arg);
            }
            Err(e) => {
                // XXX: does doing it here mess with the error report?
                func.ty = match self.global.types.get(func.ty) {
                    &Type::Func(_, ret) => ret,
                    _ => unreachable!(),
                };
                // anything, doesn't mater what, this is the easiest..
                args.push(Tree {
                    loc: Location(self.source, 0..0),
                    ty: self.global.types.number(),
                    value: TreeKind::Number(0.0),
                });

                let (actual_func, actual_arg) = if is_compose {
                    (&arg, &args[0])
                } else {
                    (&func, &arg)
                };

                let mut err_kind = err_context_complete_type(
                    &self.global.types,
                    actual_arg.loc.clone(),
                    e,
                    actual_arg.ty,
                );
                if let Some((loc, name, fty)) = coerce {
                    err_kind = err_context_auto_coerced(loc, err_kind, name.into(), fty);
                }
                // actual_func is a function:
                // see `try_apply` call sites that have a `unwrap_or_else`
                err_kind = err_context_as_nth_arg(
                    &self.global.types,
                    actual_arg.loc.clone(),
                    err_kind,
                    comma_loc,
                    actual_func,
                );
                self.report(Error(actual_func.loc.clone(), err_kind));
            }
        }
        func
    }

    fn parse_pattern(&mut self) -> (Pattern, TypeRef) {
        use TokenKind::*;

        if let term @ Token(loc, OpenBracket | TermToken!()) = self.peek_tok() {
            let loc = loc.clone();
            let err = err_unexpected(term, "a pattern", None);
            self.report(err);
            return (
                Pattern::Name(loc.clone(), "let".into()),
                self.global.types.named("paramof(typeof(let))".into()),
            );
        }

        fn symbolic_declare(
            p: &mut Parser<impl Iterator<Item = u8>>,
            name: &str,
            loc: Location,
            ty: TypeRef,
        ) {
            if let Some(e) = p
                .result
                .scope
                .declare(name.into(), ScopeItem::Binding(loc.clone(), ty))
                .map(|prev| err_already_declared(&p.global.types, name.into(), loc, prev))
            {
                p.report(e);
            }
        }

        let first_token = self.next_tok();
        let (fst, fst_ty) = match first_token.1 {
            Word(w) => {
                let ty = self.global.types.named(w.clone());
                symbolic_declare(self, &w, first_token.0.clone(), ty);
                (Pattern::Name(first_token.0, w), ty)
            }
            Bytes(v) => (Pattern::Bytes(v), {
                let fin = self.global.types.finite(true);
                self.global.types.bytes(fin)
            }),
            Number(n) => (Pattern::Number(n), self.global.types.number()),

            OpenBrace => {
                let mut items = Vec::new();
                let ty = self.global.types.named("itemof(typeof({}))".into());
                let mut rest = None;

                while match self.peek_tok() {
                    Token(_, CloseBrace) => false,

                    Token(_, Comma) if !items.is_empty() => {
                        // { ... ,,
                        self.skip_tok();
                        match (self.next_tok(), self.peek_tok()) {
                            (Token(loc, Word(w)), Token(_, CloseBrace)) => {
                                rest = Some((loc, w));
                                false
                            }
                            (other, _) => {
                                let err = err_unexpected(
                                    &other,
                                    "a name then closing '}' after ',,'",
                                    Some(&first_token),
                                );
                                self.report(err);
                                false
                            }
                        }
                    }

                    #[allow(unreachable_patterns)] // because of 'CloseBrace'
                    term @ Token(_, TermToken!()) => {
                        let err = err_unexpected(
                            term,
                            "next item or closing '}' in pattern",
                            Some(&first_token),
                        );
                        self.report(err);
                        false
                    }

                    _ => true,
                } {
                    // pattern items do not all store their location...
                    let Token(cheaty_loc, _) = self.peek_tok();
                    let cheaty_loc = cheaty_loc.clone();
                    let (item, item_ty) = self.parse_pattern();

                    // snapshot to have previous type in reported error
                    let snapshot = self.global.types.clone();
                    Type::harmonize(ty, item_ty, &mut self.global.types).unwrap_or_else(|e| {
                        self.report(err_list_type_mismatch(
                            &snapshot,
                            cheaty_loc,
                            e,
                            item_ty,
                            first_token.0.clone(),
                            ty,
                        ));
                    });
                    items.push(item);

                    if matches!(self.peek_tok(), Token(_, Comma)) {
                        self.skip_tok();
                    }
                }
                self.skip_tok();

                let fin = self.global.types.finite(rest.is_none());
                let ty = self.global.types.list(fin, ty);

                if let Some((loc, w)) = &rest {
                    symbolic_declare(self, w, loc.clone(), ty);
                }

                (Pattern::List(items, rest), ty)
            }

            Def | Let | Use | Unknown(_) => {
                let loc = first_token.0.clone();
                let err = err_unexpected(&first_token, "a pattern", None);
                let t = match &first_token.1 {
                    Def => "def",
                    Let => "let",
                    Use => "use",
                    Unknown(t) => t,
                    _ => unreachable!(),
                };
                self.report(err);
                (
                    Pattern::Name(loc, t.into()),
                    self.global.types.named(t.into()),
                )
            }

            OpenBracket | TermToken!() => unreachable!(),
        };

        if let Token(_, Equal) = self.peek_tok() {
            self.skip_tok();
            let (snd, snd_ty) = self.parse_pattern();
            (
                Pattern::Pair(Box::new(fst), Box::new(snd)),
                self.global.types.pair(fst_ty, snd_ty),
            )
        } else {
            (fst, fst_ty)
        }
    }

    fn parse_value(&mut self) -> Tree {
        use TokenKind::*;

        if let term @ Token(loc, TermToken!()) = self.peek_tok() {
            let loc = loc.clone();
            let err = err_unexpected(term, "a value", None);
            self.report(err);
            let (ty, value) = self.mktypeof("");
            return Tree { loc, ty, value };
        }

        let first_token = self.next_tok();

        let mut loc = first_token.0.clone();
        let (ty, value) = match first_token.1 {
            Word(w) => match self.result.scope.lookup(&w) {
                Some(d) => (
                    d.make_type(&mut self.global.types),
                    TreeKind::Apply(Applicable::Name(w), Vec::new()),
                ),
                None => {
                    let r = self.mktypeof(&w);
                    self.report(Error(
                        first_token.0,
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
                                let mut fake_frozen = MaybeUninit::zeroed();
                                *(fake_frozen.as_mut_ptr() as *mut usize) = r.0;
                                fake_frozen.assume_init()
                            },
                        },
                    ));
                    r
                }
            },

            Bytes(b) => (
                {
                    let fin = self.global.types.finite(true);
                    self.global.types.bytes(fin)
                },
                TreeKind::Bytes(b),
            ),
            Number(n) => (self.global.types.number(), TreeKind::Number(n)),

            OpenBracket => {
                let Tree { loc: _, ty, value } = self.parse_script();
                match self.peek_tok() {
                    Token(close_loc, CloseBracket) => {
                        loc.1.end = close_loc.1.end;
                        self.skip_tok()
                    }
                    other @ Token(_, kind) => {
                        let skip = Comma == *kind;
                        let err = err_unexpected(
                            other,
                            "next argument or closing ']'",
                            Some(&first_token),
                        );
                        self.report(err);
                        if skip {
                            self.skip_tok();
                        }
                    }
                }
                (ty, value)
            }

            OpenBrace => {
                let mut items = Vec::new();
                let ty = self.global.types.named("itemof(typeof({}))".into());

                while match self.peek_tok() {
                    Token(close_loc, CloseBrace) => {
                        loc.1.end = close_loc.1.end;
                        false
                    }

                    #[allow(unreachable_patterns)] // because of 'CloseBrace'
                    term @ Token(_, TermToken!()) => {
                        let err =
                            err_unexpected(term, "next item or closing '}'", Some(&first_token));
                        self.report(err);
                        false
                    }

                    _ => true,
                } {
                    let item = self.parse_apply();
                    let item_ty = item.ty;

                    // snapshot to have previous type in reported error
                    let snapshot = self.global.types.clone();
                    Type::harmonize(ty, item_ty, &mut self.global.types).unwrap_or_else(|e| {
                        self.report(err_list_type_mismatch(
                            &snapshot,
                            item.loc.clone(),
                            e,
                            item.ty,
                            first_token.0.clone(),
                            ty,
                        ));
                    });
                    items.push(item);

                    if matches!(self.peek_tok(), Token(_, Comma)) {
                        self.skip_tok();
                    }
                }
                self.skip_tok();

                let fin = self.global.types.finite(true);
                (self.global.types.list(fin, ty), TreeKind::List(items))
            }

            Let => {
                let parent = self.result.scope.make_into_child();
                let (pattern, pat_ty) = self.parse_pattern();

                let result = self.parse_value();
                let res_ty = result.ty;

                let fallback = if let Token(_, TermToken!()) = self.peek_tok() {
                    loc.1.end = result.loc.1.end;
                    None
                } else {
                    let then = self.parse_value();

                    // snapshot to have previous type in reported error
                    let snapshot = self.global.types.clone();
                    Type::harmonize(res_ty, then.ty, &mut self.global.types).unwrap_or_else(|e| {
                        self.report(Error(
                            then.loc.clone(),
                            ErrorKind::ContextCaused {
                                error: Box::new(Error(result.loc.clone(), e)),
                                because: ErrorContext::LetFallbackTypeMismatch {
                                    result_type: snapshot.frozen(res_ty),
                                    fallback_type: snapshot.frozen(then.ty),
                                },
                            },
                        ))
                    });

                    loc.1.end = then.loc.1.end;
                    Some(Box::new(then))
                };
                self.result.scope.restore_from_parent(parent);

                let ty = self.global.types.func(pat_ty, res_ty);
                let app = Applicable::Bind(pattern, Box::new(result), fallback);
                (ty, TreeKind::Apply(app, Vec::new()))
            }

            Def => {
                self.report(Error(first_token.0, ErrorKind::UnexpectedDefInScript));
                self.mktypeof("def")
            }

            Use => {
                self.report(Error(first_token.0, ErrorKind::UnexpectedDefInScript));
                self.mktypeof("use")
            }

            Unknown(ref w) => {
                let err = err_unexpected(&first_token, "a value", None);
                self.report(err);
                self.mktypeof(w)
            }

            TermToken!() => unreachable!(),
        };
        let fst = Tree { loc, ty, value };

        if let Token(_, Equal) = self.peek_tok() {
            self.skip_tok();
            let snd = self.parse_value();
            let mut loc = fst.loc.clone();
            loc.1.end = snd.loc.1.end;
            let ty = self.global.types.pair(fst.ty, snd.ty);
            let value = TreeKind::Pair(Box::new(fst), Box::new(snd));
            Tree { loc, ty, value }
        } else {
            fst
        }
    }

    // xxx: should it be updating the location ranges as it find more arguments?
    fn parse_apply(&mut self) -> Tree {
        use TokenKind::*;
        let mut r = self.parse_value();
        while !matches!(self.peek_tok(), Token(_, TermToken!())) {
            // "unfold" means as in this example:
            // [tonum, add 1, tostr] :42:
            //  `-> (,)((,)(tonum, add(1)), tostr)
            // `-> (,)((,)(tonum(:[52, 50]:), add(1)), tostr)
            // => tostr(add(1, tonum(:[52, 50]:)))
            fn apply_maybe_unfold(
                p: &mut Parser<impl Iterator<Item = u8>>,
                func: Tree,
                new_end: &mut usize,
            ) -> Tree {
                match func.value {
                    TreeKind::Apply(Applicable::Name(name), args) if "pipe" == name => {
                        let mut args = args.into_iter();
                        let (f, g) = (args.next().unwrap(), args.next().unwrap());
                        // (,)(f, g) => g(f(..))
                        let fx = apply_maybe_unfold(p, f, new_end);
                        // cannot fail:
                        //     with f :: a -> b and g :: b -> c
                        //     apply_maybe_unfold(p, f) :: b
                        p.try_apply(g, fx, None)
                    }
                    TreeKind::Apply(_, _)
                        if matches!(
                            p.global.types.get(func.ty),
                            Type::Func(_, _) | Type::Named(_)
                        ) =>
                    {
                        // xxx: does it need `snapshot` here?
                        let x = p.parse_value();
                        *new_end = x.loc.1.end;
                        // err_context_as_nth_arg: `func` is a function (matches ::Apply)
                        p.try_apply(func, x, None)
                    }
                    _ => {
                        let x = p.parse_value();
                        *new_end = x.loc.1.end;
                        let e = err_not_func(&p.global.types, &func);
                        p.report(Error(func.loc.clone(), e));
                        func
                    }
                }
            }
            let mut new_end = r.loc.1.end;
            r = apply_maybe_unfold(self, r, &mut new_end);
            r.loc.1.end = new_end;
        }
        r
    }

    fn parse_script(&mut self) -> Tree {
        let mut r = self.parse_apply();
        while let Token(_, TokenKind::Comma) = self.peek_tok() {
            let Token(comma_loc, _) = self.next_tok();
            let then = self.parse_apply();

            let r_ty = r.ty;
            let r_loc = r.loc.clone();

            let then_ty = then.ty;
            let then_loc = then.loc.clone();

            let (is_func, is_hole) = match self.global.types.get(then_ty) {
                Type::Func(_, _) => (true, false),
                Type::Named(_) => (false, true),
                _ => (false, false),
            };

            // `then` not a function nor a type hole
            r = if !is_func && !is_hole {
                self.report(Error(
                    r_loc,
                    ErrorKind::ContextCaused {
                        error: Box::new(Error(then_loc, err_not_func(&self.global.types, &then))),
                        because: ErrorContext::ChainedFromToNotFunc { comma_loc },
                    },
                ));
                then
            }
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            else if is_hole
                || !matches!(self.global.types.get(r_ty), Type::Func(_, _)) // note: catches coersion cases
                || Type::applicable(then_ty, r_ty, &self.global.types)
            {
                // XXX: does it need `snapshot` here?
                // err_context_as_nth_arg: we know `then` is a function
                // (if it was hole then `try_apply` mutates it)
                self.try_apply(then, r, Some(comma_loc))
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                let mut pipe = self.sure_lookup(comma_loc.clone(), "pipe");
                // XXX: does it need `snapshot` here?
                // we know that `r` is a function (see previous 'else if')
                pipe = self.try_apply(pipe, r, None);
                // err_context_as_nth_arg: `then` is either a function or a type hole
                // ('if') and it is not a type hole ('else if'), so it is a function
                self.try_apply(pipe, then, Some(comma_loc))
            };
        } // while let Comma
        r
    }

    pub fn parse(mut self) -> Processed {
        use TokenKind::*;

        // use section
        while let Token(_, Use) = self.peek_tok() {
            self.skip_tok();
            let (loc, file, name) = match (self.next_tok(), self.next_tok()) {
                (Token(loc, Bytes(file)), Token(_, Word(mut name))) => {
                    if "_" == name {
                        name.clear();
                    } else {
                        name.push('-');
                    }
                    (loc, file, name)
                }
                (Token(_, Bytes(_)), other) | (other, _) => {
                    self.report(Error(
                        other.0,
                        ErrorKind::Unexpected {
                            token: other.1,
                            expected: "file path and identifier after 'use' keyword",
                        },
                    ));
                    continue;
                }
            };
            let source = match String::from_utf8(file)
                .map_err(|e| e.to_string())
                .and_then(|s| self.global.registry.add(s).map_err(|e| e.to_string()))
            {
                Ok(bytes) => bytes,
                Err(error) => {
                    self.report(Error(loc, ErrorKind::CouldNotReadFile { error }));
                    continue;
                }
            };

            let res = process(source, self.global);
            self.result.errors.0.extend(res.errors);
            for (mut k, v) in res.scope {
                k.insert_str(0, &name);
                self.result.scope.declare(k, v);
            }
        }

        // def section
        while let Token(_, Def) = self.peek_tok() {
            self.skip_tok();
            let (loc, name, desc) = match (self.next_tok(), self.next_tok()) {
                (Token(loc, Word(name)), Token(_, Bytes(desc))) => (loc, name, desc),
                (Token(_, Word(_)), other) | (other, _) => {
                    self.report(Error(
                        other.0,
                        ErrorKind::Unexpected {
                            token: other.1,
                            expected: "name and description after 'def' keyword",
                        },
                    ));
                    continue;
                }
            };

            let val = self.parse_script();
            let desc = String::from_utf8_lossy(&desc).trim().into();
            if let Some(e) = self
                .result
                .scope
                .declare(name.clone(), ScopeItem::Defined(val, desc))
                .map(|prev| err_already_declared(&self.global.types, name, loc, prev))
            {
                self.report(e);
            }

            match self.peek_tok() {
                Token(_, Semicolon) => self.skip_tok(),
                other @ Token(_, _) => {
                    let err = err_unexpected(other, "';' after definition", None);
                    self.report(err);
                }
            }
        }

        // script section
        let mut r = if let Token(_, End) = self.peek_tok() {
            None
        } else {
            Some(self.parse_script())
        };
        if !self.result.errors.is_empty() {
            r = None;
            // note: this is the receiving end from `parse_value` when a name was not found: this
            //       is the point where we get the usizes again and compute the final FrozenType
            for e in self.result.errors.0.iter_mut() {
                if let ErrorKind::UnknownName {
                    name: _,
                    expected_type: frty,
                } = &mut e.1
                {
                    let really_usize = frty as *const FrozenType as *const usize;
                    let ty = unsafe { *really_usize };
                    mem::forget(mem::replace(frty, self.global.types.frozen(ty)));
                }
            }
        }

        self.result.tree = r;
        self.result
    }
}
// }}}

// TODO: freaking remove that, and update test!
#[cfg(test)]
impl Tree {
    #[deprecated]
    pub fn new(bytes: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        Tree::new_typed(bytes).map(|r| r.1)
    }

    #[deprecated]
    pub fn new_typed(bytes: impl IntoIterator<Item = u8>) -> Result<(FrozenType, Tree), ErrorList> {
        // XXX: won't have the prelude, so that's pretty dumb
        let mut global = Global::new();
        let r = process(
            global.registry.add_bytes("", bytes.into_iter().collect()),
            &mut global,
        );
        if r.errors.is_empty() && r.tree.is_some() {
            r.tree
                .map(|t| (global.types.frozen(t.ty), t))
                .ok_or(r.errors)
        } else {
            Err(r.errors)
        }
    }
}
