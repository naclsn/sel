use std::collections::HashMap;
use std::iter::{self, Enumerate, Peekable};
use std::mem::{self, MaybeUninit};

use crate::builtin;
use crate::error::{
    Error, ErrorContext, ErrorKind, ErrorList, Location, SourceRef, SourceRegistry,
};
use crate::types::{FrozenType, Type, TypeList, TypeRef};

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

#[derive(Debug)]
struct Scope<T> {
    parent: Option<Box<Scope<T>>>,
    names: HashMap<String, T>,
}

// lexing into tokens {{{
/// note: this is an infinite iterator (`next()` is never `None`)
pub(crate) struct Lexer<I: Iterator<Item = u8>> {
    stream: Peekable<Enumerate<I>>,
    last_loc: Option<Location>,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    pub(crate) fn new(iter: I) -> Self {
        Self {
            stream: iter.enumerate().peekable(),
            last_loc: None,
        }
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        use TokenKind::*;

        if let Some(last) = &self.last_loc {
            return Some(Token(last.clone(), End));
        }
        let Some(&(last, _)) = self.stream.peek() else {
            if self.last_loc.is_none() {
                self.last_loc = Some(Location(0));
            }
            return self.next();
        };
        let Some((loc, byte)) = self.stream.find(|c| !c.1.is_ascii_whitespace()) else {
            self.last_loc = Some(Location(last));
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
                b';' => Semicolon,

                b'#' => {
                    self.stream.find(|c| b'\n' == c.1)?;
                    return self.next();
                }
                b'_' => Word("_".into()), // ..keeping this case for those that are used to it

                c if c.is_ascii_lowercase() || b'-' == c => {
                    let w = String::from_utf8(
                        iter::once(c)
                            .chain(iter::from_fn(|| {
                                self.stream
                                    .next_if(|c| c.1.is_ascii_lowercase() || b'-' == c.1)
                                    .map(|c| c.1)
                            }))
                            .collect(),
                    )
                    .unwrap();
                    match w.as_str() {
                        "def" => Def,
                        "let" => Let,
                        "use" => Use,
                        _ => Word(w),
                    }
                }

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
struct Parser<'a, I: Iterator<Item = u8>> {
    peekable: Peekable<Lexer<I>>,
    types: TypeList,
    errors: ErrorList,
    scope: Scope<(Tree, String)>,
    source: SourceRef,
    registry: &'a mut SourceRegistry,
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
    previous: &(Tree, String),
) -> Error {
    let previous = &previous.0;
    Error(
        previous.loc.clone(),
        ErrorKind::ContextCaused {
            error: Box::new(Error(new_loc, ErrorKind::NameAlreadyDeclared { name })),
            because: ErrorContext::DeclaredHereWithType {
                with_type: types.frozen(previous.ty),
            },
        },
    )
}
// }}}

impl<T> Scope<T> {
    fn new(parent: Option<Scope<T>>) -> Scope<T> {
        Scope {
            parent: parent.map(Box::new),
            names: HashMap::new(),
        }
    }

    fn declare(&mut self, name: String, value: T) -> Option<&T> {
        let mut already = false;
        let r = self
            .names
            .entry(name)
            .and_modify(|_| already = true)
            .or_insert_with(|| value);
        if already {
            Some(r)
        } else {
            None
        }
    }

    fn lookup(&self, name: &str) -> Option<&T> {
        self.names
            .get(name)
            .or_else(|| self.parent.as_ref()?.lookup(name))
    }
}

impl<I: Iterator<Item = u8>> Parser<'_, I> {
    pub fn new(registry: &'_ mut SourceRegistry, source: SourceRef, bytes: I) -> Parser<I> {
        Parser {
            peekable: Lexer::new(bytes.into_iter()).peekable(),
            types: TypeList::default(),
            errors: ErrorList::new(),
            scope: Scope::new(None),
            source,
            registry,
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
        self.errors.push(err);
    }

    fn mktypeof(&mut self, name: &str) -> (TypeRef, TreeKind) {
        let ty = self.types.named(&format!("typeof({name})"));
        (
            ty,
            TreeKind::Apply(Applicable::Name(name.into()), Vec::new()),
        )
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

        if let Named(name) = self.types.get(func.ty) {
            let name = name.clone();
            let par = self.types.named(&format!("paramof({name})"));
            let ret = self.types.named(&format!("returnof({name})"));
            *self.types.get_mut(func.ty) = Func(par, ret);
        }

        // coerce if needed
        let Func(par_ty, _) = self.types.get(func.ty) else {
            unreachable!();
        };
        let coerce = if let Some(w) = match (self.types.get(*par_ty), self.types.get(arg.ty)) {
            (Number, Bytes(_)) => Some("tonum"),
            (Bytes(_), Number) => Some("tostr"),
            (List(_, num), Bytes(_)) if matches!(self.types.get(*num), Number) => {
                Some("codepoints")
            }
            (List(_, any), Bytes(_)) if matches!(self.types.get(*any), Bytes(_) | Named(_)) => {
                Some("graphemes")
            }
            (Bytes(_), List(_, num)) if matches!(self.types.get(*num), Number) => {
                Some("uncodepoints")
            }
            (Bytes(_), List(_, bst)) if matches!(self.types.get(*bst), Bytes(_)) => {
                Some("ungraphemes")
            }
            _ => None,
        } {
            let (mkty, _) = builtin::NAMES.get(w).unwrap();
            let coerce = Tree {
                loc: arg.loc.clone(),
                ty: mkty(&mut self.types),
                value: TreeKind::Apply(Applicable::Name(w.into()), Vec::new()),
            };
            let fty = self.types.frozen(coerce.ty);
            arg = self.try_apply(coerce, arg, None);
            Some((arg.loc.clone(), w, fty))
        } else {
            None
        };

        match Type::applied(func.ty, arg.ty, &mut self.types) {
            Ok(ret_ty) => {
                func.ty = ret_ty;
                args.push(arg);
            }
            Err(e) => {
                // XXX: does doing it here mess with the error report?
                func.ty = match self.types.get(func.ty) {
                    &Type::Func(_, ret) => ret,
                    _ => unreachable!(),
                };
                // anything, doesn't mater what, this is the easiest..
                args.push(Tree {
                    loc: Location(0),
                    ty: self.types.number(),
                    value: TreeKind::Number(0.0),
                });

                let (actual_func, actual_arg) = if is_compose {
                    (&arg, &args[0])
                } else {
                    (&func, &arg)
                };

                let mut err_kind = err_context_complete_type(
                    &self.types,
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
                    &self.types,
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
                self.types.named("paramof(typeof(let))"),
            );
        }

        fn symbolic_declare(
            p: &mut Parser<impl Iterator<Item = u8>>,
            name: &str,
            loc: Location,
            ty: TypeRef,
        ) {
            let val = Tree {
                loc: loc.clone(),
                ty,
                // anything, doesn't mater what, this is the easiest..
                value: TreeKind::Number(0.0),
            };
            if let Some(e) = p
                .scope
                .declare(name.into(), (val, String::new()))
                .map(|prev| err_already_declared(&p.types, name.into(), loc, prev))
            {
                p.report(e);
            }
        }

        let first_token = self.next_tok();
        let (fst, fst_ty) = match first_token.1 {
            Word(w) => {
                let ty = self.types.named(&w);
                symbolic_declare(self, &w, first_token.0.clone(), ty);
                (Pattern::Name(first_token.0, w), ty)
            }
            Bytes(v) => (Pattern::Bytes(v), {
                let fin = self.types.finite(true);
                self.types.bytes(fin)
            }),
            Number(n) => (Pattern::Number(n), self.types.number()),

            OpenBrace => {
                let mut items = Vec::new();
                let ty = self.types.named("itemof(typeof({}))");
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
                    let snapshot = self.types.clone();
                    Type::harmonize(ty, item_ty, &mut self.types).unwrap_or_else(|e| {
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

                let fin = self.types.finite(rest.is_none());
                let ty = self.types.list(fin, ty);

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
                (Pattern::Name(loc, t.into()), self.types.named(t))
            }

            OpenBracket | TermToken!() => unreachable!(),
        };

        if let Token(_, Equal) = self.peek_tok() {
            self.skip_tok();
            let (snd, snd_ty) = self.parse_pattern();
            (
                Pattern::Pair(Box::new(fst), Box::new(snd)),
                self.types.pair(fst_ty, snd_ty),
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

        let loc = first_token.0.clone();
        let (ty, value) = match first_token.1 {
            Word(w) => match self
                .scope
                .lookup(&w)
                .map(|d| self.types.duplicate(d.0.ty, &mut HashMap::new()))
                .or_else(|| {
                    builtin::NAMES
                        .get(&w)
                        .map(|(mkty, _)| mkty(&mut self.types))
                }) {
                Some(ty) => (ty, TreeKind::Apply(Applicable::Name(w), Vec::new())),
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
                    let fin = self.types.finite(true);
                    self.types.bytes(fin)
                },
                TreeKind::Bytes(b),
            ),
            Number(n) => (self.types.number(), TreeKind::Number(n)),

            OpenBracket => {
                let Tree { loc: _, ty, value } = self.parse_script();
                match self.peek_tok() {
                    Token(_, CloseBracket) => self.skip_tok(),
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
                let ty = self.types.named("itemof(typeof({}))");

                while match self.peek_tok() {
                    Token(_, CloseBrace) => false,

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
                    let snapshot = self.types.clone();
                    Type::harmonize(ty, item_ty, &mut self.types).unwrap_or_else(|e| {
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

                let fin = self.types.finite(true);
                (self.types.list(fin, ty), TreeKind::List(items))
            }

            Let => {
                self.scope = Scope::new(Some(mem::replace(&mut self.scope, Scope::new(None))));
                let (pattern, pat_ty) = self.parse_pattern();

                let result = self.parse_value();
                let res_ty = result.ty;

                let fallback = if let Token(_, TermToken!()) = self.peek_tok() {
                    None
                } else {
                    let then = self.parse_value();

                    // snapshot to have previous type in reported error
                    let snapshot = self.types.clone();
                    Type::harmonize(res_ty, then.ty, &mut self.types).unwrap_or_else(|e| {
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

                    Some(Box::new(then))
                };
                self.scope = *self.scope.parent.take().unwrap();

                let ty = self.types.func(pat_ty, res_ty);
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

        if let Token(equal_loc, Equal) = self.peek_tok() {
            let loc = equal_loc.clone();
            self.skip_tok();
            let snd = self.parse_value();
            let ty = self.types.pair(fst.ty, snd.ty);
            let value = TreeKind::Pair(Box::new(fst), Box::new(snd));
            Tree { loc, ty, value }
        } else {
            fst
        }
    }

    fn parse_apply(&mut self) -> Tree {
        use TokenKind::*;
        let mut r = self.parse_value();
        while !matches!(self.peek_tok(), Token(_, TermToken!())) {
            // "unfold" means as in this example:
            // [tonum, add 1, tostr] :42:
            //  `-> (,)((,)(tonum, add(1)), tostr)
            // `-> (,)((,)(tonum(:[52, 50]:), add(1)), tostr)
            // => tostr(add(1, tonum(:[52, 50]:)))
            fn apply_maybe_unfold(p: &mut Parser<impl Iterator<Item = u8>>, func: Tree) -> Tree {
                match func.value {
                    TreeKind::Apply(Applicable::Name(name), args) if "pipe" == name => {
                        let mut args = args.into_iter();
                        let (f, g) = (args.next().unwrap(), args.next().unwrap());
                        // (,)(f, g) => g(f(..))
                        let fx = apply_maybe_unfold(p, f);
                        // ucannot fail:
                        //     with f :: a -> b and g :: b -> c
                        //     apply_maybe_unfold(p, f) :: b
                        p.try_apply(g, fx, None)
                    }
                    TreeKind::Apply(_, _)
                        if matches!(p.types.get(func.ty), Type::Func(_, _) | Type::Named(_)) =>
                    {
                        // XXX: does it need `snapshot` here?
                        let x = p.parse_value();
                        // err_context_as_nth_arg: `func` is a function (matches ::Apply)
                        p.try_apply(func, x, None)
                    }
                    _ => {
                        drop(p.parse_value());
                        let e = err_not_func(&p.types, &func);
                        p.report(Error(func.loc.clone(), e));
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
        while let Token(_, TokenKind::Comma) = self.peek_tok() {
            let Token(comma_loc, _) = self.next_tok();
            let then = self.parse_apply();

            let r_ty = r.ty;
            let r_loc = r.loc.clone();

            let then_ty = then.ty;
            let then_loc = then.loc.clone();

            let (is_func, is_hole) = match self.types.get(then_ty) {
                Type::Func(_, _) => (true, false),
                Type::Named(_) => (false, true),
                _ => (false, false),
            };

            // `then` not a function nor a type hole
            r = if !is_func && !is_hole {
                self.report(Error(
                    r_loc,
                    ErrorKind::ContextCaused {
                        error: Box::new(Error(then_loc, err_not_func(&self.types, &then))),
                        because: ErrorContext::ChainedFromToNotFunc { comma_loc },
                    },
                ));
                then
            }
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            else if is_hole
                || !matches!(self.types.get(r_ty), Type::Func(_, _)) // NOTE: catches coersion cases
                || Type::applicable(then_ty, r_ty, &self.types)
            {
                // XXX: does it need `snapshot` here?
                // err_context_as_nth_arg: we know `then` is a function
                // (if it was hole then `try_apply` mutates it)
                self.try_apply(then, r, Some(comma_loc))
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                // NOTE: I've settle on compose being `f -> g -> g(f(..))` for now
                // but if the mathematical circle operator turns out better in enough
                // places I'll change this over the whole codebase on a whim
                let mut compose = Tree {
                    loc: comma_loc.clone(),
                    ty: builtin::NAMES.get("pipe").unwrap().0(&mut self.types),
                    value: TreeKind::Apply(Applicable::Name("pipe".into()), Vec::new()),
                };
                // XXX: does it need `snapshot` here?
                // we know that `r` is a function (see previous 'else if')
                compose = self.try_apply(compose, r, None);
                // err_context_as_nth_arg: `then` is either a function or a type hole
                // ('if') and it is not a type hole ('else if'), so it is a function
                self.try_apply(compose, then, Some(comma_loc))
            };
        } // while let Comma
        r
    }

    fn parse(&mut self) -> Option<Tree> {
        // use section
        while let Token(_, TokenKind::Use) = self.peek_tok() {
            let Token(loc, _) = self.next_tok();
            let (source, asname) = match (self.next_tok().1, self.next_tok().1) {
                (TokenKind::Bytes(source), TokenKind::Word(mut asname)) => {
                    if "_" == asname {
                        asname.clear();
                    } else {
                        asname.push('-');
                    }
                    (source, asname)
                }
                (TokenKind::Bytes(_), other) | (other, _) => {
                    self.report(Error(
                        loc,
                        ErrorKind::Unexpected {
                            token: other,
                            expected: "file path and identifier after 'use' keyword",
                        },
                    ));
                    continue;
                }
            };
            let bytes = String::from_utf8(source)
                .map_err(|e| e.to_string())
                .and_then(|s| {
                    let s = self.registry.push(s);
                    Ok((s, self.registry.bytes(s).map_err(|e| e.to_string())?))
                });
            let (source, bytes) = match bytes {
                Ok(bytes) => bytes,
                Err(e) => {
                    self.report(Error(
                        loc,
                        ErrorKind::CouldNotReadFile {
                            error: e.to_string(),
                        },
                    ));
                    continue;
                }
            };

            let mut sub = Parser::new(self.registry, source, bytes);
            sub.parse();
            self.errors.0.extend(sub.errors);
            self.scope
                .names
                .extend(sub.scope.names.into_iter().map(|mut a| {
                    a.0.insert_str(0, &asname);
                    a
                }));
        }

        // def section
        while let Token(_, TokenKind::Def) = self.peek_tok() {
            let Token(loc, _) = self.next_tok();
            let (name, desc) = match (self.next_tok().1, self.next_tok().1) {
                (TokenKind::Word(name), TokenKind::Bytes(desc)) => {
                    (name, String::from_utf8_lossy(&desc).into_owned())
                }
                (TokenKind::Word(_), other) | (other, _) => {
                    self.report(Error(
                        loc,
                        ErrorKind::Unexpected {
                            token: other,
                            expected: "name and description after 'def' keyword",
                        },
                    ));
                    continue;
                }
            };

            let val = self.parse_script();
            if let Some(e) = self
                .scope
                .declare(name.clone(), (val, desc))
                .map(|prev| err_already_declared(&self.types, name, loc, prev))
            {
                self.report(e);
            }

            match self.peek_tok() {
                Token(_, TokenKind::Semicolon) => self.skip_tok(),
                other @ Token(_, _) => {
                    let err = err_unexpected(other, "';' after definition", None);
                    self.report(err);
                }
            }
        }

        // script section
        let mut r = if let Token(_, TokenKind::End) = self.peek_tok() {
            None
        } else {
            Some(self.parse_script())
        };
        if !self.errors.is_empty() {
            r = None;
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
                    mem::forget(mem::replace(frty, self.types.frozen(ty)));
                }
            }
        }
        r
    }
}
// }}}

impl Tree {
    #[deprecated]
    pub fn new(bytes: impl IntoIterator<Item = u8>) -> Result<Tree, ErrorList> {
        Tree::new_typed(bytes).map(|r| r.1)
    }

    #[deprecated]
    pub fn new_typed(bytes: impl IntoIterator<Item = u8>) -> Result<(FrozenType, Tree), ErrorList> {
        let mut reg = SourceRegistry::new();
        reg.push("".into());
        let mut p = Parser::new(&mut reg, 0, bytes.into_iter());
        let r = p.parse();
        if p.errors.is_empty() && r.is_some() {
            let r = r.unwrap();
            Ok((p.types.frozen(r.ty), r))
        } else {
            Err(p.errors)
        }
    }
}
