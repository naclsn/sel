//! parsing into a CST

use std::iter::Peekable;

use crate::error::{self, Error, ErrorKind};
use crate::lex::{Lexer, Token, TokenKind};
use crate::scope::{Located, Location, SourceRef};

macro_rules! TermToken {
    () => {
        TokenKind::Comma
            | TokenKind::CloseBracket
            | TokenKind::CloseBrace
            | TokenKind::Equal // in practice it is always consumed by `parse_value`
            | TokenKind::End
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
    pub path: Box<[u8]>,
    pub loc_name: Location,
    pub name: String,
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
    /// `word ::= /[-a-z]+/ | '_'`
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
    /// `word ::= /[-a-z]+/ | '_'`
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
            Pattern::Pair { fst, snd, .. } => fst.is_refutable() && snd.is_refutable(),
            Pattern::Number { .. } | Pattern::Bytes { .. } | Pattern::List { .. } => true,
        }
    }
}

pub struct Parser<I: Iterator<Item = u8>> {
    lex: Peekable<Lexer<I>>,
    //comments: Vec<..>,
    errors: Vec<Error>,
}

impl<I: Iterator<Item = u8>> Parser<I> {
    pub fn new(source: SourceRef, bytes: I) -> Parser<I> {
        Parser {
            lex: Lexer::new(source, bytes).peekable(),
            errors: Vec::new(),
        }
    }

    pub fn errors(&self) -> &[Error] {
        &self.errors
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
        Top {
            uses: self.parse_uses(),
            defs: self.parse_defs(),
            script: if let Token(_, TokenKind::End) = self.peek_tok() {
                None
            } else {
                Some(self.parse_script())
            },
        }
    }

    pub fn parse_uses(&mut self) -> Box<[Use]> {
        let mut r = Vec::new();
        while let Token(_, TokenKind::Use) = self.peek_tok() {
            let tok_use = self.next_tok();

            let (loc_path, path, loc_name, name) = match (self.next_tok(), self.next_tok()) {
                (
                    Token(loc_path, TokenKind::Bytes(file)),
                    Token(loc_name, TokenKind::Word(mut name)),
                ) => {
                    if "_" == name {
                        name.clear();
                    } else {
                        name.push('-');
                    }
                    (loc_path, file, loc_name, name)
                }
                (Token(_, TokenKind::Bytes(_)), other) | (other, _) => {
                    self.errors.push(error::unexpected(
                        other,
                        "file path then identifier after 'use' keyword",
                        Some(tok_use),
                    ));
                    continue;
                }
            };

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(
                        other.clone(),
                        "a ',' (because of prior 'use')",
                        Some(tok_use.clone()),
                    );
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
                    self.errors.push(err);
                }
            }

            r.push(Use {
                loc_use: tok_use.0,
                loc_path,
                path,
                loc_name,
                name,
            });
        }
        r.into()
    }

    pub fn parse_defs(&mut self) -> Box<[Def]> {
        let mut r = Vec::new();
        while let Token(_, TokenKind::Def) = self.peek_tok() {
            let tok_def = self.next_tok();

            let (loc_name, name, loc_desc, desc) = match (self.next_tok(), self.next_tok()) {
                (
                    Token(loc_name, TokenKind::Word(name)),
                    Token(loc_desc, TokenKind::Bytes(desc)),
                ) => (loc_name, name, loc_desc, desc),
                (Token(_, TokenKind::Word(_)), other) | (other, _) => {
                    self.errors.push(error::unexpected(
                        other,
                        "name then description after 'def' keyword",
                        Some(tok_def),
                    ));
                    continue;
                }
            };

            // TODO(wrong todo location): maybe use description to provide more meaningfull var type names

            // TODO: https://peps.python.org/pep-0257/#handling-docstring-indentation

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(
                        other.clone(),
                        "a ',' (because of prior 'def')",
                        Some(tok_def.clone()),
                    );
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
                    self.errors.push(err);
                }
            }

            r.push(Def {
                loc_def: tok_def.0,
                loc_name,
                name,
                loc_desc,
                desc: String::from_utf8_lossy(&desc).trim().into(),
                to: self.parse_apply(),
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

                // if irrefutable, no fallback
                let alt = pat.is_refutable().then(|| {
                    let plen = self.errors.len();
                    let value = self.parse_value();

                    // try to catch and report a common mistake
                    if plen + 1 == self.errors.len() {
                        if let err @ Error(_, ErrorKind::Unexpected { .. }) =
                            self.errors.pop().expect("len >0")
                        {
                            let err = error::context_fallback_required(pat.loc(), err);
                            self.errors.push(err);
                        }
                    }

                    Box::new(value)
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
        while !matches!(self.peek_tok(), Token(_, TermToken!())) {
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
            other @ Token(somewhat_loc_close, kind) => {
                let somewhat_loc_close = somewhat_loc_close.clone();
                let skip = TokenKind::Comma == *kind;
                let err = error::unexpected(
                    other.clone(),
                    "next argument or closing ']'",
                    Some(tok_open.clone()),
                );
                self.errors.push(err);
                if skip {
                    self.skip_tok();
                }
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
        let mut loc_close = Location(0, 0..0);

        while match self.peek_tok() {
            Token(_, TokenKind::CloseBrace) => {
                loc_close = self.next_tok().0;
                false
            }

            #[allow(unreachable_patterns, reason = "because of 'CloseBrace'")]
            term @ Token(somewhat_loc_close, TermToken!()) => {
                loc_close = somewhat_loc_close.clone();
                let err = error::unexpected(
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
                            let err = error::unexpected(
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

        // logic below is too unreadable for the compiler to notice;
        // but loc_close could be left uninit and immute..
        let mut loc_close = Location(0, 0..0);

        loop {
            match self.peek_tok() {
                Token(_, TokenKind::CloseBrace) => {
                    loc_close = self.next_tok().0;
                    break;
                }

                #[allow(unreachable_patterns, reason = "because of 'CloseBrace'")]
                term @ Token(somewhat_loc_close, TermToken!()) => {
                    loc_close = somewhat_loc_close.clone();
                    let err = error::unexpected(
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
                    let err = error::unexpected(
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
                        self.errors.push(error::unexpected(
                            other.clone(),
                            "a word then closing '}' after ',,'",
                            Some(tok_open.clone()),
                        ));
                        (
                            loc_comma,
                            other.0,
                            match &other.1 {
                                TokenKind::CloseBrace => break,
                                TokenKind::Def => "?def",
                                TokenKind::Let => "?let",
                                TokenKind::Use => "?use",
                                TokenKind::Unknown(t) => t,
                                _ => "?",
                            }
                            .to_string(),
                        )
                    }
                });

                match self.peek_tok() {
                    Token(_, TokenKind::CloseBrace) => {
                        loc_close = self.next_tok().0;
                    }
                    other @ Token(somewhat_loc_close, _) => {
                        loc_close = somewhat_loc_close.clone();
                        let err = error::unexpected(
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
        // don't consume a TermToken at this point
        if let term @ Token(loc, TermToken!()) = self.peek_tok() {
            let loc = loc.clone();
            let err = error::unexpected(term.clone(), "a value", None);
            self.errors.push(err);
            return Value::Word {
                loc,
                word: "?".into(),
            };
        }

        let first_token = self.next_tok();

        let value = match first_token.1 {
            TokenKind::OpenBracket => {
                let (loc_open, subscr, loc_close) = self.parse_subscr_content(&first_token);
                Value::Subscr {
                    loc_open,
                    subscr,
                    loc_close,
                }
            }

            TokenKind::OpenBrace => {
                let (loc_open, items, rest, loc_close) = self.parse_list_content(&first_token);
                Value::List {
                    loc_open,
                    items,
                    rest,
                    loc_close,
                }
            }

            TokenKind::Number(n) => Value::Number {
                loc: first_token.0,
                number: n,
            },
            TokenKind::Bytes(b) => Value::Bytes {
                loc: first_token.0,
                bytes: b,
            },
            TokenKind::Word(w) => Value::Word {
                loc: first_token.0,
                word: w,
            },

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                self.errors
                    .push(error::unexpected(first_token.clone(), "a value", None));
                Value::Word {
                    loc: first_token.0,
                    word: match &first_token.1 {
                        TokenKind::Def => "?def",
                        TokenKind::Let => "?let",
                        TokenKind::Use => "?use",
                        TokenKind::Unknown(t) => t,
                        _ => unreachable!(),
                    }
                    .into(),
                }
            }

            TermToken!() => unreachable!(),
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
        // don't consume a TermToken at this point
        if let term @ Token(loc, TokenKind::OpenBracket | TermToken!()) = self.peek_tok() {
            let loc = loc.clone();
            let err = error::unexpected(term.clone(), "a pattern", None);
            self.errors.push(err);
            return Pattern::Word {
                loc,
                word: "?".into(),
            };
        }

        let first_token = self.next_tok();

        let pattern = match first_token.1 {
            TokenKind::OpenBrace => {
                let (loc_open, items, rest, loc_close) = self.parse_patlist_content(&first_token);
                Pattern::List {
                    loc_open,
                    items,
                    rest,
                    loc_close,
                }
            }

            TokenKind::Number(n) => Pattern::Number {
                loc: first_token.0,
                number: n,
            },
            TokenKind::Bytes(b) => Pattern::Bytes {
                loc: first_token.0,
                bytes: b,
            },
            TokenKind::Word(w) => Pattern::Word {
                loc: first_token.0,
                word: w,
            },

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                self.errors
                    .push(error::unexpected(first_token.clone(), "a pattern", None));
                Pattern::Word {
                    loc: first_token.0,
                    word: match &first_token.1 {
                        TokenKind::Def => "?def",
                        TokenKind::Let => "?let",
                        TokenKind::Use => "?use",
                        TokenKind::Unknown(t) => t,
                        _ => unreachable!(),
                    }
                    .into(),
                }
            }

            TokenKind::OpenBracket | TermToken!() => unreachable!(),
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
    use crate::scope::SourceRegistry;
    use insta::assert_debug_snapshot;

    fn t(script: &[u8]) -> (Top, Box<[Error]>) {
        let mut registry = SourceRegistry::default();
        let source = registry.add_bytes("<test>", script.iter().copied());
        let bytes = &registry.get(source).bytes;
        let mut parser = Parser::new(source, bytes.iter().copied());
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
}
