use std::iter::{self, Enumerate, Peekable};
use std::mem::{self, MaybeUninit};

use crate::error::{self, Error, ErrorContext, ErrorKind, ErrorList};
use crate::scope::{Global, Location, Scope, ScopeItem, SourceRef};
use crate::types::{FrozenType, Type, TypeRef};

pub struct Processed {
    pub errors: ErrorList,
    pub scope: Scope,
    pub tree: Option<Tree>,
}

pub fn process(source: SourceRef, global: &mut Global) -> Processed {
    let bytes = global.registry.get(source).bytes.clone();
    let p = Parser::new(source, global, bytes.into_iter());
    p.parse()
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
    End,
}

macro_rules! TermToken {
    () => {
        Comma | CloseBracket | CloseBrace | Equal | End
    };
}

#[derive(PartialEq, Debug, Clone)]
pub struct Token(pub Location, pub TokenKind);

#[derive(PartialEq, Debug, Clone)]
pub enum Applicable {
    Name(String),
    Bind(Pattern, Box<Tree>, Box<Tree>), // if pattern is infallible, fallback is garbage
}

#[derive(PartialEq, Debug, Clone)]
pub enum TreeKind {
    Number(f64),
    Bytes(Vec<u8>),
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
    Number(f64),
    Bytes(Vec<u8>),
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

            b'#' => {
                if let Some((_, b'-')) = self.stream.peek() {
                    self.stream.next();
                    let mut m = Vec::new();
                    while {
                        match self.next().unwrap().1 {
                            OpenBracket => m.push(b'b'),
                            CloseBracket if m.last().is_some_and(|b| b'b' == *b) => _ = m.pop(),
                            OpenBrace => m.push(b'B'),
                            CloseBrace if m.last().is_some_and(|b| b'B' == *b) => _ = m.pop(),
                            End => m.clear(),
                            _ => {}
                        }
                        !m.is_empty()
                    } {}
                    let n = self.next();
                    let Some(Token(_, Comma)) = n else {
                        return n;
                    };
                } else {
                    self.stream.find(|c| {
                        self.last_at = c.0;
                        b'\n' == c.1
                    });
                }
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

impl<I: Iterator<Item = u8>> Parser<'_, I> {
    pub fn new(source: SourceRef, global: &mut Global, bytes: I) -> Parser<I> {
        let scope = Scope::new(&global.scope);
        Parser {
            peekable: Lexer::new(source, bytes.into_iter()).peekable(),
            source,
            global,
            result: Processed {
                errors: ErrorList::default(),
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

    /// the following names can be looked up when parsing (ie as part of syntactical constructs):
    /// - `codepoints`
    /// - `cons`
    /// - `graphemes`
    /// - `pipe`
    /// - `tonum`
    /// - `tostr`
    /// - `uncodepoints`
    /// - `ungraphemes`
    fn sure_lookup(&mut self, loc: Location, name: &str) -> Tree {
        self.global
            .types
            .transaction_group(format!("sure_lookup '{name}'"));
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

    /// this is the receiving end from `parse_value` when a name was not found: this
    /// is the point where we get the usizes again and compute the final FrozenType
    fn fixup_errors(&mut self) {
        for e in self.result.errors.0.iter_mut() {
            if let ErrorKind::UnknownName { expected_type, .. } = &mut e.1 {
                let really_usize = expected_type as *const FrozenType as *const TypeRef;
                let ty = unsafe { *really_usize };
                mem::forget(mem::replace(expected_type, self.global.types.frozen(ty)));
            }
        }
    }

    fn fatal_reached(&mut self) -> ! {
        use std::io::IsTerminal;
        if std::env::var("SEL_FATAL").is_ok() {
            let mut n = 0;
            let use_color = std::io::stderr().is_terminal();
            for e in self.result.errors.iter() {
                eprintln!("{}", e.report(&self.global.registry, use_color));
                n += 1;
            }
            eprintln!("({n} error{})", if 1 == n { "" } else { "s" });
        } else {
            eprintln!("note: run with `SEL_FATAL=1` to show parsing diagonistic at this point");
        }
        panic!("fatal encoutered - stopping there");
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

        self.global.types.transaction_group(match base {
            Applicable::Name(name) => format!("try_apply '{name}' {} <- {}", func.ty, arg.ty),
            Applicable::Bind(_, _, _) => format!("try_apply let {} <- {}", func.ty, arg.ty),
        });

        if let Named(name) = self.global.types.get(func.ty) {
            let name = name.clone();
            // XXX: 'paramof' and 'returnof' pseudo syntaxes are somewhat temporary
            let par = self.global.types.named(format!("paramof({name})"));
            let ret = self.global.types.named(format!("returnof({name})"));
            // raw `set` is alright because `func.ty` is a named
            self.global.types.set(func.ty, Func(par, ret));
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

        //// snapshot to have previous type in reported error
        //let snapshot = self.global.types.clone();
        match Type::applied(func.ty, arg.ty, &mut self.global.types) {
            Ok(ret_ty) => {
                func.ty = ret_ty;
                args.push(arg);
            }

            Err(e) => {
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

                let mut err_kind = error::context_complete_type(
                    &self.global.types,
                    actual_arg.loc.clone(),
                    e,
                    actual_arg.ty,
                );
                if let Some((loc, name, fty)) = coerce {
                    err_kind = error::context_auto_coerced(loc, err_kind, name.into(), fty);
                }
                // actual_func is a function:
                // see `try_apply` call sites that have a `unwrap_or_else`
                err_kind = error::context_as_nth_arg(
                    &self.global.types,
                    actual_arg.loc.clone(),
                    err_kind,
                    comma_loc,
                    actual_func,
                );
                self.report(Error(actual_func.loc.clone(), err_kind));

                func.ty = match self.global.types.get(func.ty) {
                    &Type::Func(_, ret) => ret,
                    _ => unreachable!(),
                };
            }
        }
        func
    }

    fn parse_pattern(&mut self) -> (Pattern, TypeRef) {
        use TokenKind::*;

        if let term @ Token(loc, OpenBracket | TermToken!()) = self.peek_tok() {
            let loc = loc.clone();
            let err = error::unexpected(term, "a pattern", None);
            self.report(err);
            return (
                Pattern::Name(loc.clone(), "let".into()),
                // XXX: 'paramof' pseudo syntax is somewhat temporary
                self.global.types.named("paramof(let)".into()),
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
                .map(|prev| error::already_declared(&p.global.types, name.into(), loc, prev))
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
            Number(n) => (Pattern::Number(n), self.global.types.number()),
            Bytes(v) => (Pattern::Bytes(v), {
                let fin = self.global.types.finite(true);
                self.global.types.bytes(fin)
            }),

            OpenBrace => {
                let mut items = Vec::new();
                let ty = self.global.types.named("item".into());
                let mut rest = None;

                while match self.peek_tok() {
                    Token(_, CloseBrace) => false,

                    // `if !empty`: there must be something before ',,'
                    Token(_, Comma) if !items.is_empty() => {
                        // { ... ,,
                        self.skip_tok();
                        match (self.next_tok(), self.peek_tok()) {
                            (Token(loc, Word(w)), Token(_, CloseBrace)) => {
                                rest = Some((loc, w));
                                false
                            }
                            (other, _) => {
                                let err = error::unexpected(
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
                        let err = error::unexpected(
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
                        self.report(error::list_type_mismatch(
                            &snapshot,
                            cheaty_loc,
                            e,
                            item_ty,
                            first_token.0.clone(),
                            ty,
                        ));
                    });
                    items.push(item);

                    match self.peek_tok() {
                        Token(_, Comma) => self.skip_tok(),
                        Token(_, CloseBrace) => break,
                        other => {
                            let err = error::unexpected(
                                other,
                                "',' before next item or closing '}' in pattern",
                                Some(&first_token),
                            );
                            self.report(err);
                        }
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
                let err = error::unexpected(&first_token, "a pattern", None);
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
            let err = error::unexpected(term, "a value", None);
            self.report(err);
            let ty = self.global.types.named("?".into());
            let value = TreeKind::Apply(Applicable::Name("".into()), Vec::new());
            return Tree { loc, ty, value };
        }

        let first_token = self.next_tok();

        let mut loc = first_token.0.clone();
        let (ty, value) = match first_token.1 {
            Word(fatal) if "fatal" == fatal => {
                self.report(Error(
                    loc,
                    ErrorKind::CouldNotReadFile {
                        error: "fatal reached".into(),
                    },
                ));
                self.fixup_errors();
                self.fatal_reached();
            }
            Word(w) => match self.result.scope.lookup(&w) {
                Some(d) => {
                    self.global
                        .types
                        .transaction_group(format!("lookup '{w}' => Some(..)"));
                    (
                        d.make_type(&mut self.global.types),
                        TreeKind::Apply(Applicable::Name(w), Vec::new()),
                    )
                }
                None => {
                    self.global
                        .types
                        .transaction_group(format!("lookup '{w}' => None"));
                    let r = (
                        self.global.types.named(w.clone()),
                        TreeKind::Apply(Applicable::Name(w.clone()), Vec::new()),
                    );
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

            Number(n) => (self.global.types.number(), TreeKind::Number(n)),
            Bytes(b) => (
                {
                    let fin = self.global.types.finite(true);
                    self.global.types.bytes(fin)
                },
                TreeKind::Bytes(b),
            ),

            OpenBracket => {
                let Tree { loc: _, ty, value } = self.parse_script();
                match self.peek_tok() {
                    Token(close_loc, CloseBracket) => {
                        loc.1.end = close_loc.1.end;
                        self.skip_tok()
                    }
                    other @ Token(_, kind) => {
                        let skip = Comma == *kind;
                        let err = error::unexpected(
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
                self.global.types.transaction_group("parse list".into());
                let mut items = Vec::new();
                let ty = self.global.types.named("item".into());
                let mut rest = None;

                while match self.peek_tok() {
                    Token(close_loc, CloseBrace) => {
                        loc.1.end = close_loc.1.end;
                        false
                    }

                    // `if !empty`: there must be something before ',,'
                    Token(_, Comma) if !items.is_empty() => {
                        // { ... ,,
                        self.skip_tok();
                        rest = Some(self.parse_value());
                        match self.peek_tok() {
                            Token(close_loc, CloseBrace) => {
                                loc.1.end = close_loc.1.end;
                                panic!("all good: {rest:#?}");
                                //false
                            }
                            other => {
                                let err = error::unexpected(
                                    &other,
                                    "closing '}' after ',,'",
                                    Some(&first_token),
                                );
                                self.report(err);
                                false
                            }
                        }
                    }

                    #[allow(unreachable_patterns)] // because of 'CloseBrace'
                    term @ Token(_, TermToken!()) => {
                        let err =
                            error::unexpected(term, "next item or closing '}'", Some(&first_token));
                        self.report(err);
                        false
                    }

                    _ => true,
                } {
                    let item = self.parse_apply();
                    let item_ty = item.ty;

                    // snapshot to have previous type in reported error
                    let snapshot = self.global.types.clone();
                    self.global.types.transaction_group("list harmonize".into());
                    Type::harmonize(ty, item_ty, &mut self.global.types).unwrap_or_else(|e| {
                        self.report(error::list_type_mismatch(
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

                if let Some(rest) = rest {
                    todo!("multiple cons with {items:?} {rest:?}");
                } else {
                    let fin = self.global.types.finite(true);
                    (self.global.types.list(fin, ty), TreeKind::List(items))
                }
            }

            Def | Let | Use | Unknown(_) => {
                let err = error::unexpected(&first_token, "a value", None);
                self.report(err);
                let name = match &first_token.1 {
                    Def => "?def",
                    Let => "?let",
                    Use => "?use",
                    Unknown(t) => t,
                    _ => unreachable!(),
                };
                (
                    self.global.types.named(name.into()),
                    TreeKind::Apply(Applicable::Name(name.into()), Vec::new()),
                )
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

    // XXX: should it be updating the location ranges as it find more arguments?
    fn parse_apply(&mut self) -> Tree {
        use TokenKind::*;

        let mut r = match self.peek_tok() {
            Token(loc, Let) => {
                let mut loc = loc.clone();
                self.skip_tok();

                let parent = self.result.scope.make_into_child();
                self.global.types.transaction_group("let pattern".into());
                let (pattern, pat_ty) = self.parse_pattern();

                let result = self.parse_value();
                let res_ty = result.ty;

                fn is_irrefutable(pattern: &Pattern) -> bool {
                    match pattern {
                        Pattern::Number(_) | Pattern::Bytes(_) | Pattern::List(_, _) => false,
                        Pattern::Name(_, _) => true,
                        Pattern::Pair(fst, snd) => is_irrefutable(fst) && is_irrefutable(snd),
                    }
                }

                // if irrefutable, no fallback
                let fallback = if is_irrefutable(&pattern) {
                    Tree {
                        loc: Location(0, 0..0),
                        ty: 0,
                        value: TreeKind::Number(0.0),
                    }
                    //unsafe { MaybeUninit::zeroed().assume_init() }
                } else {
                    let fallback = self.parse_value();

                    self.global.types.transaction_group("let harmonize".into());
                    // snapshot to have previous type in reported error
                    let snapshot = self.global.types.clone();
                    Type::harmonize(res_ty, fallback.ty, &mut self.global.types).unwrap_or_else(
                        |e| {
                            self.report(Error(
                                fallback.loc.clone(),
                                ErrorKind::ContextCaused {
                                    error: Box::new(Error(result.loc.clone(), e)),
                                    because: ErrorContext::LetFallbackTypeMismatch {
                                        result_type: snapshot.frozen(res_ty),
                                        fallback_type: snapshot.frozen(fallback.ty),
                                    },
                                },
                            ))
                        },
                    );

                    loc.1.end = fallback.loc.1.end;
                    fallback
                };

                self.result.scope.restore_from_parent(parent);

                self.global.types.transaction_group(format!(
                    "finish let pattern; pattern type: {pat_ty}, result type: {res_ty}"
                ));
                let ty = self.global.types.func(pat_ty, res_ty);
                let app = Applicable::Bind(pattern, Box::new(result), Box::new(fallback));
                let value = TreeKind::Apply(app, Vec::new());
                Tree { loc, ty, value }
            }
            _ => self.parse_value(),
        };

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
                        let x = p.parse_value();
                        *new_end = x.loc.1.end;
                        // err_context_as_nth_arg: `func` is a function (matches ::Apply)
                        p.try_apply(func, x, None)
                    }

                    _ => {
                        let x = p.parse_value();
                        *new_end = x.loc.1.end;
                        let e = error::not_func(&p.global.types, &func);
                        p.report(Error(func.loc.clone(), e));
                        func
                    }
                }
            }

            let mut new_end = r.loc.1.end;
            r = apply_maybe_unfold(self, r, &mut new_end);
            r.loc.1.end = new_end;
        } // while not TermToken
        r
    }

    fn parse_script(&mut self) -> Tree {
        let mut r = self.parse_apply();
        while let Token(_, TokenKind::Comma) = self.peek_tok() {
            let Token(comma_loc, _) = self.next_tok();
            let then = self.parse_apply();

            let (r_is_func, r_is_hole) = match self.global.types.get(r.ty) {
                Type::Func(_, _) => (true, false),
                Type::Named(_) => (false, true),
                _ => (false, false),
            };
            let (then_is_func, then_is_hole) = match self.global.types.get(then.ty) {
                Type::Func(_, _) => (true, false),
                Type::Named(_) => (false, true),
                _ => (false, false),
            };

            // `then` not a function nor a type hole
            r = if !then_is_func && !then_is_hole {
                self.report(Error(
                    r.loc.clone(),
                    ErrorKind::ContextCaused {
                        error: Box::new(Error(
                            then.loc.clone(),
                            error::not_func(&self.global.types, &then),
                        )),
                        because: ErrorContext::ChainedFromToNotFunc { comma_loc },
                    },
                ));
                then
            }
            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            else if
            // is likely an `arg, func` situation if r not a function and either:
            // - r neither func nor a hole: has to be an arg (whever err it causes)
            // - hole at this position (after ',') has to be a func
            // note: catches some coersion cases
            !r_is_func && (!r_is_hole || then_is_hole)
                // if r is a hole, making it an arg actually disables certains useful situations
                // TODO: figure it out
                    || !r_is_hole && Type::applicable(then.ty, r.ty, &self.global.types)
            {
                // err_context_as_nth_arg: we know `then` is a function
                // (if it was hole then `try_apply` mutates it)
                self.try_apply(then, r, Some(comma_loc))
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                let mut pipe = self.sure_lookup(comma_loc.clone(), "pipe");
                // we know that `r` is a function or hole (see previous 'else if')
                pipe = self.try_apply(pipe, r, None);
                // err_context_as_nth_arg: `then` is either a function or a type hole
                // ('if') and it is not a type hole ('else if'), so it is a function
                pipe = self.try_apply(pipe, then, Some(comma_loc));
                pipe
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
            match String::from_utf8(file)
                .map_err(|e| e.to_string())
                .and_then(|s| self.global.registry.add(s).map_err(|e| e.to_string()))
            {
                Ok(source) => {
                    let res = process(source, self.global);
                    self.result.errors.0.extend(res.errors);
                    for (mut k, v) in res.scope {
                        k.insert_str(0, &name);
                        self.result.scope.declare(k, v);
                    }
                }
                Err(error) => {
                    self.report(Error(loc, ErrorKind::CouldNotReadFile { error }));
                }
            }

            match self.peek_tok() {
                Token(_, Comma | End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(other, "a ',' (because of prior `use`)", None);
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
                    self.report(err);
                }
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
            // TODO: maybe use description to provide more meaningfull var type names

            // note: we are faking like the name is already defined; so we first parse a value
            // which may report some unknown name error, then we go through these and resolve
            // these that regard the name currently being defined
            // this approach is used over declaring a `Named(name)` because it makes it possible to
            // work and harmonize with previous uses of `name` (in the
            // `result->errors->retain_mut`)
            let val = self.parse_value();
            let desc = String::from_utf8_lossy(&desc).trim().into();

            let def_name = name.clone();
            let def_ty = val.ty;
            let def_loc = val.loc.clone();

            if let Some(e) = self
                .result
                .scope
                .declare(name.clone(), ScopeItem::Defined(val, desc))
                .map(|prev| error::already_declared(&self.global.types, name, loc.clone(), prev))
            {
                self.report(e);
            } else {
                let mut types = Vec::new();

                self.result.errors.0.retain_mut(|e| match &mut e.1 {
                    ErrorKind::UnknownName {
                        name,
                        expected_type,
                    } if &def_name == name => {
                        // note: this is one receiving end from `parse_value`:
                        //       error is removed as name is now defined
                        let really_usize = expected_type as *const FrozenType as *const TypeRef;
                        let ty = unsafe { *really_usize };
                        mem::forget(mem::replace(expected_type, FrozenType::Number)); // make it valid

                        self.global.types.transaction_group(format!(
                            "'{def_name}' now defined; type in error: {ty}, parsed value type: {def_ty}"
                        ));
                        // todo: maybe snapshot?
                        if Type::harmonize(ty, def_ty, &mut self.global.types).is_err() {
                            types.push((e.0.clone(), self.global.types.frozen(ty)));
                        }

                        false
                    }
                    _ => true,
                });

                self.global
                    .types
                    .transaction_group(format!("finish def '{def_name}'"));

                if !types.is_empty() {
                    let with_type = self.global.types.frozen(def_ty);
                    let e = Error(
                        def_loc.clone(),
                        ErrorKind::ContextCaused {
                            error: Box::new(Error(def_loc, ErrorKind::InconsistentType { types })),
                            because: ErrorContext::DeclaredHereWithType { with_type },
                        },
                    );
                    self.report(e);
                }
            }

            match self.peek_tok() {
                Token(_, Comma | End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(other, "a ',' (because of prior `def`)", None);
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
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
            self.fixup_errors();
        }

        self.result.tree = r;
        self.result
    }
}
// }}}
