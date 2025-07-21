/// parsing into a CST
use std::iter::Peekable;

use crate::error::{self, Error, ErrorKind};
use crate::lex::{Lexer, Token, TokenKind};
use crate::scope::SourceRef;

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
    pub uses: Vec<Use>,
    pub defs: Vec<Def>,
    pub script: Option<Script>,
}
#[derive(Debug, Clone)]
pub struct Use {
    pub path: Vec<u8>,
    pub name: String,
}
#[derive(Debug, Clone)]
pub struct Def {
    pub name: String,
    pub desc: String,
    pub to: Apply,
}

/// `script ::= <apply> {',' <apply>}`
#[derive(Debug, Clone)]
pub struct Script {
    pub head: Apply,
    pub tail: Vec<Apply>,
}

/// `apply ::= (<binding> | <value>) {<value>}`
#[derive(Debug, Clone)]
pub struct Apply {
    pub base: ApplyBase,
    pub args: Vec<Value>,
}
#[derive(Debug, Clone)]
pub enum ApplyBase {
    /// `binding ::= 'let' (<irrefut> <value> | <pattern> <value> <value>)`
    Binding(Pattern, Box<Value>, Box<Value>), // garbage if `pat.is_irrefutable()`
    /// `value ::= <atom> | <subscr> | <list> | <pair>`
    Value(Box<Value>),
}

/// `value ::= <atom> | <subscr> | <list> | <pair>`
#[derive(Debug, Clone)]
pub enum Value {
    /// `subscr ::= '[' <script> ']'`
    Subscr(Script),

    /// `list ::= '{' [<apply> {',' <apply>} [',' [',' <apply>]]] '}'`
    List(Vec<Apply>, Option<Box<Apply>>),
    /// `pair ::= (<atom> | <subscr> | <list>) '=' <value>`
    Pair(Box<Value>, Box<Value>),

    /// `atom ::= <word> | <bytes> | <number>`
    /// `word ::= /[-a-z]+/ | '_'`
    Word(String),
    /// `atom ::= <word> | <bytes> | <number>`
    /// `bytes ::= /:([^:]|::)*:/`
    Bytes(Vec<u8>),
    /// `atom ::= <word> | <bytes> | <number>`
    /// `number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)?/`
    Number(f64),
}

/// `irrefut ::= <word> | <irrefut> '=' <irrefut>`
/// `pattern ::= <atom> | <patlist> | <patpair>`
#[derive(Debug, Clone)]
pub enum Pattern {
    /// `atom ::= <word> | <bytes> | <number>`
    /// `word ::= /[-a-z]+/ | '_'`
    Word(String),
    /// `atom ::= <word> | <bytes> | <number>`
    /// `bytes ::= /:([^:]|::)*:/`
    Bytes(Vec<u8>),
    /// `atom ::= <word> | <bytes> | <number>`
    /// `number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)?/`
    Number(f64),

    /// `patlist ::= '{' [<pattern> {',' <pattern>} [',' [',' <word>]]] '}'`
    List(Vec<Pattern>, Option<String>),
    /// `patpair ::= (<atom> | <patlist>) '=' <pattern>`
    Pair(Box<Pattern>, Box<Pattern>),
}

impl Pattern {
    pub fn is_irrefutable(&self) -> bool {
        match self {
            Pattern::Word(_) => true,
            Pattern::Pair(fst, snd) => fst.is_irrefutable() && snd.is_irrefutable(),
            Pattern::Number(_) | Pattern::Bytes(_) | Pattern::List(_, _) => false,
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

    pub fn parse_uses(&mut self) -> Vec<Use> {
        let mut r = Vec::new();
        while let Token(_, TokenKind::Use) = self.peek_tok() {
            self.skip_tok();

            let (_loc, path, name) = match (self.next_tok(), self.next_tok()) {
                (Token(loc, TokenKind::Bytes(file)), Token(_, TokenKind::Word(mut name))) => {
                    if "_" == name {
                        name.clear();
                    } else {
                        name.push('-');
                    }
                    (loc, file, name)
                }
                (Token(_, TokenKind::Bytes(_)), other) | (other, _) => {
                    self.errors.push(error::unexpected(
                        &other,
                        "file path then identifier after 'use' keyword",
                        None,
                    ));
                    continue;
                }
            };

            r.push(Use { path, name });

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(other, "a ',' (because of prior 'use')", None);
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
                    self.errors.push(err);
                }
            }
        }
        r
    }

    pub fn parse_defs(&mut self) -> Vec<Def> {
        let mut r = Vec::new();
        while let Token(_, TokenKind::Def) = self.peek_tok() {
            self.skip_tok();

            let (_loc, name, desc) = match (self.next_tok(), self.next_tok()) {
                (Token(loc, TokenKind::Word(name)), Token(_, TokenKind::Bytes(desc))) => {
                    (loc, name, desc)
                }
                (Token(_, TokenKind::Word(_)), other) | (other, _) => {
                    self.errors.push(error::unexpected(
                        &other,
                        "name then description after 'def' keyword",
                        None,
                    ));
                    continue;
                }
            };

            // TODO(wrong todo location): maybe use description to provide more meaningfull var type names

            // TODO: https://peps.python.org/pep-0257/#handling-docstring-indentation

            r.push(Def {
                name,
                desc: String::from_utf8_lossy(&desc).trim().into(),
                to: self.parse_apply(),
            });

            match self.peek_tok() {
                Token(_, TokenKind::Comma | TokenKind::End) => self.skip_tok(),
                other @ Token(_, kind) => {
                    let err = error::unexpected(other, "a ',' (because of prior 'def')", None);
                    if matches!(kind, TermToken!()) {
                        self.skip_tok();
                    }
                    self.errors.push(err);
                }
            }
        }
        r
    }

    pub fn parse_script(&mut self) -> Script {
        let head = self.parse_apply();
        let mut tail = Vec::new();
        while let Token(_comma_loc, TokenKind::Comma) = self.peek_tok() {
            self.skip_tok();
            tail.push(self.parse_apply());
        }
        Script { head, tail }
    }

    pub fn parse_apply(&mut self) -> Apply {
        let base = match self.peek_tok() {
            Token(let_loc, TokenKind::Let) => {
                let let_loc = let_loc.clone();
                self.skip_tok();

                let pat = self.parse_pattern();
                let res = Box::new(self.parse_value());

                // if irrefutable, no fallback
                let alt = if pat.is_irrefutable() {
                    Box::new(Value::Number(0.0))
                } else {
                    let plen = self.errors.len();
                    let value = self.parse_value();

                    // try to catch and report a common mistake
                    if plen + 1 == self.errors.len() {
                        if let err @ Error(_, ErrorKind::Unexpected { .. }) =
                            self.errors.pop().expect("len >0")
                        {
                            let err = error::context_fallback_required(let_loc, err);
                            self.errors.push(err);
                        }
                    }

                    Box::new(value)
                };

                ApplyBase::Binding(pat, res, alt)
            }

            _ => ApplyBase::Value(Box::new(self.parse_value())),
        };

        let mut args = Vec::new();
        while !matches!(self.peek_tok(), Token(_, TermToken!())) {
            args.push(self.parse_value());
        }

        Apply { base, args }
    }

    fn parse_subscr_content(&mut self, first_token: &Token) -> Script {
        let subscr = self.parse_script();

        match self.peek_tok() {
            Token(_close_loc, TokenKind::CloseBracket) => {
                //loc.1.end = close_loc.1.end;
                self.skip_tok()
            }
            other @ Token(_, kind) => {
                let skip = TokenKind::Comma == *kind;
                let err =
                    error::unexpected(other, "next argument or closing ']'", Some(&first_token));
                self.errors.push(err);
                if skip {
                    self.skip_tok();
                }
            }
        }

        subscr
    }

    fn parse_list_content(&mut self, first_token: &Token) -> (Vec<Apply>, Option<Box<Apply>>) {
        let mut items = Vec::new();
        let mut rest = None;

        while match self.peek_tok() {
            Token(_close_loc, TokenKind::CloseBrace) => {
                //loc.1.end = close_loc.1.end;
                self.skip_tok();
                false
            }

            #[allow(unreachable_patterns, reason = "because of 'CloseBrace'")]
            term @ Token(_, TermToken!()) => {
                let err = error::unexpected(term, "next item or closing '}'", Some(first_token));
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
                    self.skip_tok();
                    rest = Some(Box::new(self.parse_apply()));
                    // parse_apply ends on a TermToken

                    match self.peek_tok() {
                        Token(_close_loc, TokenKind::CloseBrace) => {
                            //loc.1.end = close_loc.1.end;
                            self.skip_tok();
                        }
                        other => {
                            let err = error::unexpected(
                                other,
                                "closing '}' after ',,'",
                                Some(&first_token),
                            );
                            self.errors.push(err);
                        }
                    }

                    break;
                } // if ',,'
            } // if ','
        } // while

        (items, rest)
    }

    pub fn parse_value(&mut self) -> Value {
        // don't consume a TermToken at this point
        if let term @ Token(_loc, TermToken!()) = self.peek_tok() {
            let err = error::unexpected(term, "a value", None);
            self.errors.push(err);
            return Value::Word("?".into());
        }

        let first_token = self.next_tok();

        //let mut loc = first_token.0.clone();
        let value = match first_token.1 {
            TokenKind::OpenBracket => {
                let subscr = self.parse_subscr_content(&first_token);
                Value::Subscr(subscr)
            }

            TokenKind::OpenBrace => {
                let (items, rest) = self.parse_list_content(&first_token);
                Value::List(items, rest)
            }

            TokenKind::Word(w) => Value::Word(w),
            TokenKind::Bytes(b) => Value::Bytes(b),
            TokenKind::Number(n) => Value::Number(n),

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                self.errors
                    .push(error::unexpected(&first_token, "a value", None));
                Value::Word(
                    match &first_token.1 {
                        TokenKind::Def => "?def",
                        TokenKind::Let => "?let",
                        TokenKind::Use => "?use",
                        TokenKind::Unknown(t) => t,
                        _ => unreachable!(),
                    }
                    .into(),
                )
            }

            TermToken!() => unreachable!(),
        };

        if let Token(_, TokenKind::Equal) = self.peek_tok() {
            self.skip_tok();
            Value::Pair(Box::new(value), Box::new(self.parse_value()))
        } else {
            value
        }
    }

    pub fn parse_pattern(&mut self) -> Pattern {
        // don't consume a TermToken at this point
        if let term @ Token(_loc, TokenKind::OpenBracket | TermToken!()) = self.peek_tok() {
            let err = error::unexpected(term, "a pattern", None);
            self.errors.push(err);
            return Pattern::Word("?".into());
        }

        let first_token = self.next_tok();

        let pattern = match first_token.1 {
            TokenKind::OpenBrace => {
                let mut items = Vec::new();
                let mut rest = None;

                loop {
                    match self.peek_tok() {
                        Token(_close_loc, TokenKind::CloseBrace) => {
                            self.skip_tok();
                            break;
                        }

                        #[allow(unreachable_patterns, reason = "because of 'CloseBrace'")]
                        term @ Token(_, TermToken!()) => {
                            let err = error::unexpected(
                                term,
                                "next item or closing '}'",
                                Some(&first_token),
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
                                other,
                                "',' between items or closing '}'",
                                Some(&first_token),
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
                        self.skip_tok();
                        // { ... ,,

                        rest = Some(match self.next_tok() {
                            Token(_, TokenKind::Word(w)) => w,
                            other => {
                                self.errors.push(error::unexpected(
                                    &other,
                                    "a word then closing '}' after ',,'",
                                    Some(&first_token),
                                ));
                                match &other.1 {
                                    TokenKind::CloseBrace => break,
                                    TokenKind::Def => "?def",
                                    TokenKind::Let => "?let",
                                    TokenKind::Use => "?use",
                                    TokenKind::Unknown(t) => t,
                                    _ => "?",
                                }
                                .into()
                            }
                        });

                        match self.peek_tok() {
                            Token(_close_loc, TokenKind::CloseBrace) => {
                                self.skip_tok();
                            }
                            other => {
                                let err = error::unexpected(
                                    other,
                                    "closing '}' after ',,'",
                                    Some(&first_token),
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

                Pattern::List(items, rest)
            }

            TokenKind::Word(w) => Pattern::Word(w),
            TokenKind::Bytes(b) => Pattern::Bytes(b),
            TokenKind::Number(n) => Pattern::Number(n),

            TokenKind::Def | TokenKind::Let | TokenKind::Use | TokenKind::Unknown(_) => {
                self.errors
                    .push(error::unexpected(&first_token, "a pattern", None));
                Pattern::Word(
                    match &first_token.1 {
                        TokenKind::Def => "?def",
                        TokenKind::Let => "?let",
                        TokenKind::Use => "?use",
                        TokenKind::Unknown(t) => t,
                        _ => unreachable!(),
                    }
                    .into(),
                )
            }

            TokenKind::OpenBracket | TermToken!() => unreachable!(),
        };

        if let Token(_, TokenKind::Equal) = self.peek_tok() {
            self.skip_tok();
            Pattern::Pair(Box::new(pattern), Box::new(self.parse_pattern()))
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
        (parser.parse_top(), parser.errors.into_boxed_slice())
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
