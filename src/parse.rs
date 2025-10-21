//! parsing into a CST

use std::iter::Peekable;

use crate::errors::{self, Error};
use crate::lex::{Lexer, Token, TokenKind};
use crate::module::Location;

macro_rules! TermTokenNoEnd {
    () => {
        TokenKind::Comma | TokenKind::CloseBracket | TokenKind::CloseBrace | TokenKind::Equal
        // in practice '=' is always consumed by `parse_value`
    };
}
macro_rules! TermToken {
    () => {
        TermTokenNoEnd!() | TokenKind::End
    };
}

/// `top ::= {'use' <bytes> <word> ','} {'def' <word> <bytes> <value> ','} [<script>]`
#[derive(Debug, Clone)]
pub struct Top {
    pub uses: Box<[Use]>,
    pub defs: Box<[Def]>,
    pub script: Option<Script>,
}
#[derive(Debug, Clone)]
pub struct Use {
    pub loc_use: Location,
    pub loc_path: Location,
    pub path: Box<str>,
    pub loc_prefix: Location,
    pub prefix: String,
}
#[derive(Debug, Clone)]
pub struct Def {
    pub loc_def: Location,
    pub loc_name: Location,
    pub name: String,
    pub loc_desc: Location,
    pub desc: String,
    pub to: Apply,
}

/// `script ::= <apply> {',' <apply>}`
#[derive(Debug, Clone)]
pub struct Script {
    pub head: Apply,
    pub tail: Box<[(Location, Apply)]>, // each with locatio of the ','
}

/// `apply ::= (<binding> | <value>) {<value>}`
#[derive(Debug, Clone)]
pub struct Apply {
    pub base: ApplyBase,
    pub args: Box<[Value]>,
}
#[derive(Debug, Clone)]
pub enum ApplyBase {
    /// `binding ::= 'let' (<irrefut> <value> | <pattern> <value> <value>)`
    Binding {
        loc_let: Location,
        pat: Pattern,
        res: Box<Value>,
        alt: Option<Box<Value>>, // Some(.) iff `pat.is_refutable()`
    },
    /// `value ::= <atom> | <subscr> | <list> | <pair>`
    Value(Box<Value>),
}

/// `value ::= <atom> | <subscr> | <list> | <pair>`
#[derive(Debug, Clone)]
pub enum Value {
    /// `atom ::= <word> | <bytes> | <number>`
    /// `number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)?/`
    Number { loc: Location, number: f64 },
    /// `atom ::= <word> | <bytes> | <number>`
    /// `bytes ::= /:([^:]|::)*:/`
    Bytes { loc: Location, bytes: Box<[u8]> },
    /// `atom ::= <word> | <bytes> | <number>`
    /// `word ::= /[-a-z]+/`
    Word { loc: Location, word: String },

    /// `subscr ::= '[' <script> ']'`
    Subscr {
        loc_open: Location,
        subscr: Script,
        loc_close: Location,
    },

    /// `list ::= '{' [<apply> {',' <apply>} [',' [',' <apply>]]] '}'`
    List {
        loc_open: Location,
        items: Box<[Apply]>,
        rest: Option<(Location, Box<Apply>)>, // with location of the extra ','
        loc_close: Location,
    },
    /// `pair ::= (<atom> | <subscr> | <list>) '=' <value>`
    Pair {
        fst: Box<Value>,
        loc_equal: Location,
        snd: Box<Value>,
    },
}

/// `irrefut ::= <word> | <irrefut> '=' <irrefut>`
/// `pattern ::= <atom> | <patlist> | <patpair>`
#[derive(Debug, Clone)]
pub enum Pattern {
    /// `atom ::= <word> | <bytes> | <number>`
    /// `number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)?/`
    Number { loc: Location, number: f64 },
    /// `atom ::= <word> | <bytes> | <number>`
    /// `bytes ::= /:([^:]|::)*:/`
    Bytes { loc: Location, bytes: Box<[u8]> },
    /// `atom ::= <word> | <bytes> | <number>`
    /// `word ::= /[-a-z]+/`
    Word { loc: Location, word: String },

    /// `patlist ::= '{' [<pattern> {',' <pattern>} [',' [',' <word>]]] '}'`
    List {
        loc_open: Location,
        items: Box<[Pattern]>,
        rest: Option<(Location, Location, String)>, // with location of the extra ',' and word
        loc_close: Location,
    },
    /// `patpair ::= (<atom> | <patlist>) '=' <pattern>`
    Pair {
        fst: Box<Pattern>,
        loc_equal: Location,
        snd: Box<Pattern>,
    },
}

impl Pattern {
    pub fn is_refutable(&self) -> bool {
        match self {
            Pattern::Word { .. } => false,
            Pattern::Pair { fst, snd, .. } => fst.is_refutable() || snd.is_refutable(),
            Pattern::Number { .. } | Pattern::Bytes { .. } | Pattern::List { .. } => true,
        }
    }
}

// loc trait and impls {{{
// TODO: maybe overkill idk
pub trait Located {
    fn loc(&self) -> Location;
}

fn loc_span(start: &Location, end: &Location) -> Location {
    assert_eq!(
        start.0, end.0,
        "loc_span crosses source boundary..? {start:?} - {end:?}",
    );
    Location(start.0.clone(), start.1.start..end.1.end)
}

impl Located for Top {
    fn loc(&self) -> Location {
        let scr = self.script.as_ref().map(|s| s.loc());
        let start = None
            .or_else(|| self.uses.first().map(|u| u.loc()))
            .or_else(|| self.defs.first().map(|d| d.loc()))
            .or(scr.clone());
        let end = scr
            .or_else(|| self.defs.last().map(|d| d.loc()))
            .or_else(|| self.uses.last().map(|u| u.loc()));
        match (start, end) {
            (Some(start), Some(end)) => loc_span(&start, &end),
            (Some(one), None) | (None, Some(one)) => one,
            (None, None) => unreachable!("ig idk, but empty file if so"),
        }
    }
}
impl Located for Use {
    fn loc(&self) -> Location {
        loc_span(&self.loc_use, &self.loc_prefix)
    }
}
impl Located for Def {
    fn loc(&self) -> Location {
        loc_span(&self.loc_def, &self.to.loc())
    }
}
impl Located for Script {
    fn loc(&self) -> Location {
        let head = self.head.loc();
        match self.tail.last() {
            Some((_, app)) => loc_span(&head, &app.loc()),
            None => head,
        }
    }
}
impl Located for Apply {
    fn loc(&self) -> Location {
        let base = self.base.loc();
        match self.args.last() {
            Some(arg) => loc_span(&base, &arg.loc()),
            None => base,
        }
    }
}
impl Located for ApplyBase {
    fn loc(&self) -> Location {
        match self {
            ApplyBase::Binding {
                loc_let, res, alt, ..
            } => loc_span(
                loc_let,
                &match alt {
                    Some(alt) => alt.loc(),
                    None => res.loc(),
                },
            ),
            ApplyBase::Value(value) => value.loc(),
        }
    }
}
impl Located for Value {
    fn loc(&self) -> Location {
        match self {
            Value::Number { loc, .. } => loc.clone(),
            Value::Bytes { loc, .. } => loc.clone(),
            Value::Word { loc, .. } => loc.clone(),
            Value::Subscr {
                loc_open,
                loc_close,
                ..
            } => loc_span(loc_open, loc_close),
            Value::List {
                loc_open,
                loc_close,
                ..
            } => loc_span(loc_open, loc_close),
            Value::Pair { fst, snd, .. } => loc_span(&fst.loc(), &snd.loc()),
        }
    }
}
impl Located for Pattern {
    fn loc(&self) -> Location {
        match self {
            Pattern::Number { loc, .. } => loc.clone(),
            Pattern::Bytes { loc, .. } => loc.clone(),
            Pattern::Word { loc, .. } => loc.clone(),
            Pattern::List {
                loc_open,
                loc_close,
                ..
            } => loc_span(loc_open, loc_close),
            Pattern::Pair { fst, snd, .. } => loc_span(&fst.loc(), &snd.loc()),
        }
    }
}
// }}}

pub struct Parser<'parse, I: Iterator<Item = u8>> {
    lex: Peekable<Lexer<'parse, I>>,
    //comments: Vec<..>,
    errors: Vec<Error>,
}

impl<'parse, I: Iterator<Item = u8>> Parser<'parse, I> {
    pub fn new(path: &'parse str, bytes: I) -> Self {
        Parser {
            lex: Lexer::new(path, bytes).peekable(),
            errors: Vec::new(),
        }
    }

    #[allow(dead_code)]
    pub fn errors(&self) -> &[Error] {
        &self.errors
    }
    #[allow(dead_code)]
    pub fn boxed_errors(self) -> Box<[Error]> {
        self.errors.into()
    }

    fn peek_tok(&mut self) -> &Token {
        self.lex.peek().expect("unreachable: infinite iterator")
    }
    fn next_tok(&mut self) -> Token {
        self.lex.next().expect("unreachable: infinite iterator")
    }
    fn skip_tok(&mut self) {
        self.lex.next();
    }

    pub fn parse_top(&mut self) -> Top {
        // after either of `parse_uses` and `parse_defs` we could be on a syntax error
        // that matches TermToken; in which case it would fall all the way until
        // `parse_value` which will not consume it and make the whole parser give up
        // right away, hence the skip loops after each (and also before while at it..)
        if matches!(self.peek_tok().1, TermTokenNoEnd!()) {
            let wrong = self.next_tok();
            self.errors.push(errors::unexpected(
                wrong,
                "'use', 'def' or something, but not that",
                None,
            ));
            while matches!(self.peek_tok().1, TermTokenNoEnd!()) {
                self.skip_tok();
            }
        }

        let uses = self.parse_uses();
        while matches!(self.peek_tok().1, TermTokenNoEnd!()) {
            self.skip_tok();
        }

        let defs = self.parse_defs();
        while matches!(self.peek_tok().1, TermTokenNoEnd!()) {
            self.skip_tok();
        }

        let script = (!matches!(self.peek_tok().1, TokenKind::End)).then(|| self.parse_script());
        // TODO: catch suspicious use of an unknown token "_" and report with how it's not a type
        // hole but any ident would be one idk

        Top { uses, defs, script }
    }

    pub fn parse_uses(&mut self) -> Box<[Use]> {
        let mut r = Vec::new();
        while match self.peek_tok() {
            Token(_, TokenKind::Use) => true,
            other @ Token(_, TokenKind::Comma) => {
                let other = other.clone();
                self.errors.push(errors::unexpected(
                    other,
                    "a single ',' between 'use's",
                    None,
                ));
                // skip any TermToken but still accept to continue
                while matches!(self.peek_tok().1, TermTokenNoEnd!()) {
                    self.skip_tok();
                }
                matches!(self.peek_tok().1, TokenKind::Use)
            }
            _ => false,
        } {
            let tok_use = self.next_tok();

            let (loc_path, path, loc_prefix, prefix) = match (self.next_tok(), self.next_tok()) {
                (
                    Token(loc_path, TokenKind::Bytes(path)),
                    Token(loc_prefix, TokenKind::Word(prefix)),
                ) => (loc_path, path, loc_prefix, prefix),
                (Token(loc_path, TokenKind::Bytes(path)), other) => {
                    self.errors.push(errors::unexpected(
                        other.clone(),
                        "file path then identifier after 'use' keyword",
                        Some(tok_use.clone()),
                    ));
                    if matches!(other.1, TokenKind::Comma | TokenKind::End) {
                        continue;
                    }
                    (loc_path, path, other.0.clone(), other.1.to_string())
                }
                (other, Token(loc_prefix, TokenKind::Word(prefix))) => {
                    let somewhat_loc_path = other.0.clone();
                    let not_path = other.1.to_string().as_bytes().into();
                    self.errors.push(errors::unexpected(
                        other,
                        "file path then identifier after 'use' keyword",
                        Some(tok_use.clone()),
                    ));
                    (somewhat_loc_path, not_path, loc_prefix, prefix)
                }
                (other, Token(_, maybe_term)) => {
                    self.errors.push(errors::unexpected(
                        other,
                        "file path then identifier after 'use' keyword",
                        Some(tok_use),
                    ));
                    if !matches!(maybe_term, TokenKind::Comma | TokenKind::End)
                        && matches!(self.peek_tok().1, TokenKind::Comma | TokenKind::End)
                    {
                        self.skip_tok();
                    }
                    continue;
                }
            };

            let path: Box<str> = match str::from_utf8(&path) {
                Ok(_) => unsafe { std::str::from_boxed_utf8_unchecked(path) },
                Err(err) => {
                    let lossy = String::from_utf8_lossy(&path).into_owned();
                    self.errors
                        .push(errors::broken_utf8(loc_path.clone(), path, err));
                    lossy.into_boxed_str()
                }
            };

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other => {
                    let other = other.clone();
                    self.errors.push(errors::unexpected(
                        other,
                        "a ',' (because of prior 'use')",
                        Some(tok_use.clone()),
                    ))
                }
            }

            r.push(Use {
                loc_use: tok_use.0,
                loc_path,
                path,
                loc_prefix,
                prefix,
            });
        }
        r.into()
    }

    pub fn parse_defs(&mut self) -> Box<[Def]> {
        let mut r = Vec::new();
        while match self.peek_tok() {
            Token(_, TokenKind::Def) => true,
            other @ Token(_, TokenKind::Comma) => {
                let other = other.clone();
                self.errors.push(errors::unexpected(
                    other,
                    "a single ',' between 'def's",
                    None,
                ));
                // skip any TermToken but still accept to continue
                while matches!(self.peek_tok().1, TermTokenNoEnd!()) {
                    self.skip_tok();
                }
                // (we only go so far, if this a 'use' then whever-)
                matches!(self.peek_tok().1, TokenKind::Def)
            }
            other @ Token(_, TokenKind::Use) => {
                let other = other.clone();
                self.errors.push(errors::unexpected(
                    other,
                    "'use's to all be before the first 'def'",
                    None,
                ));
                false
            }
            _ => false,
        } {
            let tok_def = self.next_tok();

            let (loc_name, name, loc_desc, desc) = match (self.next_tok(), self.next_tok()) {
                (
                    Token(loc_name, TokenKind::Word(name)),
                    Token(loc_desc, TokenKind::Bytes(desc)),
                ) => (loc_name, name, loc_desc, desc),
                (Token(loc_name, TokenKind::Word(name)), other) => {
                    let somewhat_loc_desc = other.0.clone();
                    let not_desc = other.1.to_string().as_bytes().into();
                    self.errors.push(errors::unexpected(
                        other.clone(),
                        "name then description after 'def' keyword",
                        Some(tok_def.clone()),
                    ));
                    if matches!(other.1, TokenKind::Comma | TokenKind::End) {
                        continue;
                    }
                    (loc_name, name, somewhat_loc_desc, not_desc)
                }
                (other, Token(loc_desc, TokenKind::Bytes(desc))) => {
                    self.errors.push(errors::unexpected(
                        other.clone(),
                        "name then description after 'def' keyword",
                        Some(tok_def.clone()),
                    ));
                    (other.0.clone(), other.1.to_string(), loc_desc, desc)
                }
                (other, other2) => {
                    self.errors.push(errors::unexpected(
                        other.clone(),
                        "name then description after 'def' keyword",
                        Some(tok_def.clone()),
                    ));
                    if matches!(other2.1, TokenKind::Comma | TokenKind::End) {
                        continue;
                    }
                    (
                        other.0,
                        other.1.to_string(),
                        other2.0,
                        other2.1.to_string().as_bytes().into(),
                    )
                }
            };

            let to = self.parse_apply();

            // TODO(wrong todo location): maybe use description to provide more meaningfull var type names
            // TODO: https://peps.python.org/pep-0257/#handling-docstring-indentation

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other => {
                    let other = other.clone();
                    self.errors.push(errors::unexpected(
                        other,
                        "a ',' (because of prior 'def')",
                        Some(tok_def.clone()),
                    ))
                }
            }

            r.push(Def {
                loc_def: tok_def.0,
                loc_name,
                name,
                loc_desc,
                desc: String::from_utf8_lossy(&desc).trim().into(),
                to,
            });
        }
        r.into()
    }

    pub fn parse_script(&mut self) -> Script {
        let head = self.parse_apply();
        let mut tail = Vec::new();
        while let Token(_, TokenKind::Comma) = self.peek_tok() {
            let loc_comma = self.next_tok().0;
            tail.push((loc_comma, self.parse_apply()));
        }
        Script {
            head,
            tail: tail.into(),
        }
    }

    pub fn parse_apply(&mut self) -> Apply {
        let base = match self.peek_tok() {
            Token(_, TokenKind::Let) => {
                let tok_let = self.next_tok();

                let pat = self.parse_pattern();
                let res = Box::new(self.parse_value());

                // if refutable, fallback expected
                let alt = pat.is_refutable().then(|| {
                    // try to catch and report a common mistake: missing fallback
                    Box::new(if let term @ Token(loc, TermToken!()) = self.peek_tok() {
                        let loc = loc.clone();
                        let err = errors::context_fallback_required(
                            pat.loc(),
                            errors::unexpected(term.clone(), "a fallback value", None),
                        );
                        self.errors.push(err);
                        Value::Word {
                            loc,
                            word: "?".into(),
                        }
                    } else {
                        self.parse_value()
                    })
                });

                ApplyBase::Binding {
                    loc_let: tok_let.0,
                    pat,
                    res,
                    alt,
                }
            }

            _ => ApplyBase::Value(Box::new(self.parse_value())),
        };

        let mut args = Vec::new();
        // don't eat a 'def' so we can recover from a missing ',' after a full def
        while !matches!(self.peek_tok(), Token(_, TermToken!() | TokenKind::Def)) {
            args.push(self.parse_value());
        }

        Apply {
            base,
            args: args.into(),
        }
    }

    fn parse_subscr_content(&mut self, tok_open: &Token) -> (Location, Script, Location) {
        let loc_open = tok_open.0.clone();

        let subscr = self.parse_script();

        let loc_close = match self.peek_tok() {
            Token(loc_close, TokenKind::CloseBracket) => {
                let loc_close = loc_close.clone();
                self.skip_tok();
                loc_close
            }
            other @ Token(somewhat_loc_close, _) => {
                let somewhat_loc_close = somewhat_loc_close.clone();
                let err = errors::unexpected(
                    other.clone(),
                    "next argument or closing ']'",
                    Some(tok_open.clone()),
                );
                self.errors.push(err);
                somewhat_loc_close
            }
        };

        (loc_open, subscr, loc_close)
    }

    fn parse_list_content(
        &mut self,
        tok_open: &Token,
    ) -> (
        Location,
        Box<[Apply]>,
        Option<(Location, Box<Apply>)>,
        Location,
    ) {
        let loc_open = tok_open.0.clone();

        let mut items = Vec::new();
        let mut rest = None;

        // logic below is too unreadable for the compiler to notice;
        // but loc_close could be left uninit and immute..
        let mut loc_close = Location(Default::default(), 0..0);

        while match self.peek_tok() {
            Token(_, TokenKind::CloseBrace) => {
                loc_close = self.next_tok().0;
                false
            }

            comma @ Token(_, TokenKind::Comma) => {
                let err = errors::unexpected(
                    comma.clone(),
                    "no extra comma at this point",
                    Some(tok_open.clone()),
                );
                self.errors.push(err);
                true
            }

            #[allow(unreachable_patterns, reason = "because of 'Comma' and 'CloseBrace'")]
            term @ Token(somewhat_loc_close, TermToken!()) => {
                loc_close = somewhat_loc_close.clone();
                let err = errors::unexpected(
                    term.clone(),
                    "next item or closing '}'",
                    Some(tok_open.clone()),
                );
                self.errors.push(err);
                false
            }

            _ => true,
        } {
            items.push(self.parse_apply());
            // parse_apply ends on a TermToken

            if matches!(self.peek_tok(), Token(_, TokenKind::Comma)) {
                self.skip_tok();
                // { ... ,,
                if matches!(self.peek_tok(), Token(_, TokenKind::Comma)) {
                    let loc_comma = self.next_tok().0;
                    rest = Some((loc_comma, Box::new(self.parse_apply())));
                    // parse_apply ends on a TermToken

                    match self.peek_tok() {
                        Token(_, TokenKind::CloseBrace) => {
                            loc_close = self.next_tok().0;
                        }
                        other @ Token(somewhat_loc_close, _) => {
                            loc_close = somewhat_loc_close.clone();
                            let err = errors::unexpected(
                                other.clone(),
                                "closing '}' after ',,'",
                                Some(tok_open.clone()),
                            );
                            self.errors.push(err);
                        }
                    }

                    break;
                } // if ',,'
            } // if ','
        } // while

        (loc_open, items.into(), rest, loc_close)
    }

    fn parse_patlist_content(
        &mut self,
        tok_open: &Token,
    ) -> (
        Location,
        Box<[Pattern]>,
        Option<(Location, Location, String)>,
        Location,
    ) {
        let loc_open = tok_open.0.clone();

        let mut items = Vec::new();
        let mut rest = None;

        let loc_close;

        loop {
            match self.peek_tok() {
                Token(_, TokenKind::CloseBrace) => {
                    loc_close = self.next_tok().0;
                    break;
                }

                comma @ Token(_, TokenKind::Comma) => {
                    let err = errors::unexpected(
                        comma.clone(),
                        "no extra comma at this point",
                        Some(tok_open.clone()),
                    );
                    self.errors.push(err);
                }

                #[allow(unreachable_patterns, reason = "because of 'Comma' and 'CloseBrace'")]
                term @ Token(somewhat_loc_close, TermToken!()) => {
                    loc_close = somewhat_loc_close.clone();
                    let err = errors::unexpected(
                        term.clone(),
                        "next item or closing '}'",
                        Some(tok_open.clone()),
                    );
                    self.errors.push(err);
                    break;
                }

                _ => (), // continue
            }

            items.push(self.parse_pattern());

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::CloseBrace) => (),
                other => {
                    let err = errors::unexpected(
                        other.clone(),
                        "',' between items or closing '}'",
                        Some(tok_open.clone()),
                    );
                    self.errors.push(err);
                }
            }

            // { ... ,
            if matches!(self.peek_tok(), Token(_, TokenKind::Comma)) {
                self.skip_tok();
                if !matches!(self.peek_tok(), Token(_, TokenKind::Comma)) {
                    continue;
                }
                let loc_comma = self.next_tok().0;
                // { ... ,,

                rest = Some(match self.next_tok() {
                    Token(loc_rest, TokenKind::Word(w)) => (loc_comma, loc_rest, w),
                    other => {
                        self.errors.push(errors::unexpected(
                            other.clone(),
                            "a word then closing '}' after ',,'",
                            Some(tok_open.clone()),
                        ));
                        (loc_comma, other.0, other.1.to_string())
                    }
                });

                match self.peek_tok() {
                    Token(_, TokenKind::CloseBrace) => {
                        loc_close = self.next_tok().0;
                    }
                    other @ Token(somewhat_loc_close, _) => {
                        loc_close = somewhat_loc_close.clone();
                        let err = errors::unexpected(
                            other.clone(),
                            "closing '}' after ',,'",
                            Some(tok_open.clone()),
                        );
                        self.errors.push(err);
                        // should this branch continue or break? it can be 1 of 2 situations:
                        // - the intended '}' /is present/ later on, so continue is better
                        // - there is no corresponding '}' *at all*, in which case the risk is to eat the whole file...
                    }
                }
                break;
            } // if ',', then !!','
        } // loop

        (loc_open, items.into(), rest, loc_close)
    }

    pub fn parse_value(&mut self) -> Value {
        // the TermToken branch needs to not consume; hence the peek here and the individual nexts
        let first_tok = self.peek_tok();
        let value = match &first_tok.1 {
            TokenKind::OpenBracket => {
                let open = self.next_tok();
                let (loc_open, subscr, loc_close) = self.parse_subscr_content(&open);
                Value::Subscr {
                    loc_open,
                    subscr,
                    loc_close,
                }
            }

            TokenKind::OpenBrace => {
                let open = self.next_tok();
                let (loc_open, items, rest, loc_close) = self.parse_list_content(&open);
                Value::List {
                    loc_open,
                    items,
                    rest,
                    loc_close,
                }
            }

            TokenKind::Number(n) => Value::Number {
                number: *n,
                loc: self.next_tok().0,
            },
            TokenKind::Bytes(b) => Value::Bytes {
                bytes: b.clone(),
                loc: self.next_tok().0,
            },
            TokenKind::Word(w) => Value::Word {
                word: w.clone(),
                loc: self.next_tok().0,
            },

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                let other = self.next_tok();
                let loc = other.0.clone();
                let word = other.1.to_string();
                self.errors.push(errors::unexpected(other, "a value", None));
                Value::Word { loc, word }
            }

            TermToken!() => {
                let loc = first_tok.0.clone();
                let word = "?".into();
                let err = errors::unexpected(first_tok.clone(), "a value", None);
                self.errors.push(err);
                return Value::Word { loc, word };
            }
        };

        if let Token(_, TokenKind::Equal) = self.peek_tok() {
            let loc_equal = self.next_tok().0;
            Value::Pair {
                fst: Box::new(value),
                loc_equal,
                snd: Box::new(self.parse_value()),
            }
        } else {
            value
        }
    }

    pub fn parse_pattern(&mut self) -> Pattern {
        // the TermToken branch needs to not consume; hence the peek here and the individual nexts
        let first_tok = self.peek_tok();
        let pattern = match &first_tok.1 {
            TokenKind::OpenBrace => {
                let open = self.next_tok();
                let (loc_open, items, rest, loc_close) = self.parse_patlist_content(&open);
                Pattern::List {
                    loc_open,
                    items,
                    rest,
                    loc_close,
                }
            }

            TokenKind::Number(n) => Pattern::Number {
                number: *n,
                loc: self.next_tok().0,
            },
            TokenKind::Bytes(b) => Pattern::Bytes {
                bytes: b.clone(),
                loc: self.next_tok().0,
            },
            TokenKind::Word(w) => Pattern::Word {
                word: w.clone(),
                loc: self.next_tok().0,
            },

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                let other = self.next_tok();
                let loc = other.0.clone();
                let word = other.1.to_string();
                self.errors
                    .push(errors::unexpected(other.clone(), "a pattern", None));
                Pattern::Word { loc, word }
            }

            TokenKind::OpenBracket | TermToken!() => {
                let loc = first_tok.0.clone();
                let word = "?".into();
                let err = errors::unexpected(first_tok.clone(), "a pattern", None);
                self.errors.push(err);
                return Pattern::Word { loc, word };
            }
        };

        if let Token(_, TokenKind::Equal) = self.peek_tok() {
            let loc_equal = self.next_tok().0;
            Pattern::Pair {
                fst: Box::new(pattern),
                loc_equal,
                snd: Box::new(self.parse_pattern()),
            }
        } else {
            pattern
        }
    }
}

#[test]
fn test() {
    use insta::assert_debug_snapshot;

    fn t(script: &[u8]) -> (Top, Box<[Error]>) {
        let mut parser = Parser::new("<test>", script.iter().copied());
        (parser.parse_top(), parser.errors.into())
    }

    assert_debug_snapshot!(t(b""));
    assert_debug_snapshot!(t(b"-"));
    assert_debug_snapshot!(t(b"- 2"));
    assert_debug_snapshot!(t(b"2 -"));
    assert_debug_snapshot!(t(b"-, split:-:, map[tonum, add1, tostr], join:+:"));
    assert_debug_snapshot!(t(b"tonum, add234121, tostr, ln"));
    assert_debug_snapshot!(t(b"[tonum, add234121, tostr] :13242:"));
    assert_debug_snapshot!(t(b"{repeat 1, {}}"));
    assert_debug_snapshot!(t(b"{repeat 1,, {}}"));
    assert_debug_snapshot!(t(b"{0,, repeat 1}"));
    assert_debug_snapshot!(t(b"a=b"));
    assert_debug_snapshot!(t(b"a=b=c"));
    assert_debug_snapshot!(t(b"a=[b=c]"));
    assert_debug_snapshot!(t(b"[a=b]=c"));
    assert_debug_snapshot!(t(b"let a let b a b a # note: is syntax error"));
    assert_debug_snapshot!(t(b"let {a b, c} 0"));
    assert_debug_snapshot!(t(b"let 0=a a :ok:"));
    // not quite a syntax error but only out of luck:
    // ```sel
    // [let a
    //     [[let b
    //         a
    //     ]                           # b -> a
    //         [panic: unreachable:]]  # a
    // ]                               # a -> a
    //     [panic: unreachable:]       # a
    // ```
    assert_debug_snapshot!(t(b"
let a
    [let b
        a
        [panic: unreachable:]]
    [panic: unreachable:]
"));
    assert_debug_snapshot!(t(b"use :path: name"));
    assert_debug_snapshot!(t(b"
use :path/a: a,, # extra comma
use :path/b: b # missing comma
use :path/c: _,
use path name,
use :actual path:, # not even registered
use :again path: 42, # registered with a bad name
use not-bytes, # not even registered
use not-bytes 123123 # not even registered
"));
    assert_debug_snapshot!(t(b"def succ: y, maths ppl, just y?: add 1,"));
    assert_debug_snapshot!(t(b"
def a :: b,, # extra comma
def b :: a # missing comma
def x y z,
def 42 :: 42,
def :wrong: way around,
def _XOPEN_SOURCE, # seen as `def _ XOPEN _ SOURCE` anyways
def 1, # does not eat the ',' (does not define anything)
def z, # does not eat the ',' (does not define anything)
hello
"));
    assert_debug_snapshot!(t(b"use :a: a, def b:: b, use :c: c, d")); // loose 'use' after 'def'
    assert_debug_snapshot!(t(b"]}] 42")); // loose TermTokenNoEnd before
    assert_debug_snapshot!(t(b"use :path: name]}] 42")); // 'use' with loose TermTokenNoEnd after
    assert_debug_snapshot!(t(b"def name :desc: 12]}] 42")); // 'def' with loose TermTokenNoEnd after
    assert_debug_snapshot!(t(b"use :path: name]}] def name :desc: 12]}] 42"));
    assert_debug_snapshot!(t(b"x[x {a b,, c d, y]y"));
    assert_debug_snapshot!(t(b"
use :p: n,
def f: d: v,
[let a={h,, t] 1 0
"));
    assert_debug_snapshot!(t(b"let let={:let:,,LET} 3 3"));
    assert_debug_snapshot!(t(b"[subscr without closing"));
    assert_debug_snapshot!(t(b"x {, let {,}} y"));
    assert_debug_snapshot!(t(b"x [  {  ] [  let { ] y"));
}
