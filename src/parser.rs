use std::{str::Chars, iter::Peekable};
use crate::{engine::{Value, Apply, Function, Typed}, prelude::{lookup_name, lookup_unary, lookup_binary}};

#[derive(Debug, Clone, Copy)]
pub enum Unop {
    Array,
    Flip,
}

#[derive(Debug, Clone, Copy)]
pub enum Binop {
    Addition,
    Substraction,
    Multiplication,
    Division,
    Range,
}

#[derive(Debug, Clone, Copy)]
pub enum Operator {
    Unary(Unop),
    Binary(Binop),
}

#[derive(Debug, Clone)]
enum Token {
    Name(String),
    Literal(Value),
    Operator(Operator),
    SubScript(Vec<Token>),
    Composition,
}

struct Lexer<'a> { source: Peekable<Chars<'a>> }
fn lex_string<'a>(script: &'a String) -> Lexer<'a> { Lexer { source: script.chars().peekable() } }

impl<'a> Iterator for Lexer<'a> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        match self.source.peek() {
            None => None,

            Some(c) if c.is_ascii_whitespace() => {
                // self.source.next();
                while !self.source
                    .next_if(|cc| cc.is_ascii_whitespace())
                    .is_none()
                {}
                self.next()
            },

            Some('#') => {
                self.source
                    .by_ref()
                    .skip_while(|cc| '\n' != *cc)
                    .next();
                self.next()
            },

            Some(c) if c.is_ascii_digit() => {
                let mut acc: String = "".to_string();
                loop {
                    match self.source.next_if(|cc| cc.is_ascii_digit()) {
                        Some(cc) => { acc.push(cc); }
                        None => { break; }
                    }
                }
                Some(Token::Literal(Value::Num(acc.parse().unwrap())))
            },

            Some(c) if c.is_ascii_lowercase() => {
                let mut acc: String = "".to_string();
                loop {
                    match self.source.next_if(|cc| cc.is_ascii_lowercase()) {
                        Some(cc) => { acc.push(cc); }
                        None => { break; }
                    }
                }
                Some(Token::Name(acc))
            },

            Some('{') => {
                let mut lvl: u32 = 0;
                let acc = self.source
                    .by_ref()
                    .take_while(|cc| {
                        if '{' == *cc {
                            lvl+= 1;
                        }
                        if '}' == *cc {
                            assert!(0 < lvl, "Unbalanced string literal: missing closing '}}'");
                            lvl-= 1;
                        }
                        return '}' != *cc;
                    })
                    .skip(1)
                    .collect();
                assert!(0 == lvl, "Unbalanced string literal: missing openning '{{'");
                Some(Token::Literal(Value::Str(acc)))
            },

            Some('[') => {
                let vec = lex_string(&self.source
                        .by_ref()
                        .skip(1)
                        .take_while(|cc| ']' != *cc) // YYY: crap, does not catch missing closing ']'
                        .collect()
                    )
                    .collect();
                Some(Token::SubScript(vec))
            },

            Some(c) => {
                let r = match c {
                    ',' => Some(Token::Composition),
                    '+' => Some(Token::Operator(Operator::Binary(Binop::Addition))),
                    '-' => Some(Token::Operator(Operator::Binary(Binop::Substraction))),
                    '.' => Some(Token::Operator(Operator::Binary(Binop::Multiplication))),
                    '/' => Some(Token::Operator(Operator::Binary(Binop::Division))),
                    ':' => Some(Token::Operator(Operator::Binary(Binop::Range))),
                    '@' => Some(Token::Operator(Operator::Unary(Unop::Array))),
                    '%' => Some(Token::Operator(Operator::Unary(Unop::Flip))),
                    c => todo!("Unhandled character '{c}'"),
                };
                self.source.next();
                r
            },

        } // match peek
    } // fn next
} // impl Iterator for Lexer

pub(crate) struct Parser<'a> { lexer: Peekable<Lexer<'a>> }
pub(crate) fn parse_string<'a>(script: &'a String) -> Parser<'a> { Parser { lexer: lex_string(script).peekable() } }
fn parse_vec<'a>(_tokens: &'a Vec<Token>) -> Parser<'a> { todo!("parser on vec (used for groupings)") }

// YYY: remove (no, you don't need it...)
trait Ensure {
    fn ensure(self, msg: &str) -> Self;
}
impl<T> Ensure for Option<T> {
    fn ensure(self, msg: &str) -> Self {
        Some(self.expect(msg))
    }
}

impl Parser<'_> {
    /// Build each functions and return a single
    /// function that will apply its input to each
    /// in order.
    pub fn result(&mut self) -> Value {
        let fs: Vec<Value> = self
            .filter(|f| {
                match f {
                    Value::Fun(_) => true,
                    other => {
                        println!("non-function value in script:");
                        println!("  found value of type: {}", other.typed());
                        panic!("type error")
                    },
                }
            })
            // .map(|f| { println!("got {}", f.typed()); f })
            .collect();

        let firstin = fs
            .first()
            .map(|f| {
                match f {
                    Value::Fun(f) => f.maps.0.clone(),
                    _ => unreachable!(),
                }
            })
            .unwrap();
        let lastout = fs
            .last()
            .map(|f| {
                match f {
                    Value::Fun(f) => f.maps.1.clone(),
                    _ => unreachable!(),
                }
            })
            .unwrap();

        Value::Fun(
            Function {
                maps: (firstin, lastout),
                args: fs,
                func: |_, args| {
                    let arg = args
                        .last()
                        .unwrap()
                        .clone();
                    args[..args.len()-1]
                        .iter()
                        .fold(arg, |r, f| f
                            .clone()
                            .apply(r))
                },
            }
        )

    } // fn result

    /// atom ::= literal
    ///        | name
    ///        | unop (binop | atom)
    ///        | binop atom
    ///        | subscript
    /// subscript ::= '[' _elements1 ']'
    fn next_atom(&mut self) -> Option<Value> {
        match self.lexer.next() {
            None | Some(Token::Composition) => None,
            Some(Token::Literal(value)) => Some(value),

            Some(Token::Name(name)) =>
                lookup_name(name)
                    .ensure("Unknown name"),

            Some(Token::Operator(Operator::Unary(un))) =>
                Some(lookup_unary(un).apply(
                    if let Some(Token::Operator(Operator::Binary(bin))) = self.lexer.peek() {
                        lookup_binary(*bin) // copy
                    } else {
                        self.next_atom()
                            .expect("Missing argument for unary")
                    }
                )),

            Some(Token::Operator(Operator::Binary(bin))) =>
                Some(lookup_binary(bin)
                    .apply(self
                        .next_atom()
                        .expect("Missing argument for binary")
                    )
                ),

            Some(Token::SubScript(tokens)) =>
                Some(parse_vec(&tokens).result()),

        } // match lexer next
    } // next_atom
} // impl Parser

impl<'a> Iterator for Parser<'a> {
    type Item = Value;

    /// script ::= _elements1
    /// _elements1 ::= element {',' element} [',']
    /// element ::= atom {atom}
    fn next(&mut self) -> Option<Self::Item> {
        match self.next_atom() {
            None => None,

            Some(mut base) => {
                loop {
                    match self.next_atom() {
                        None  => { break; },
                        Some(arg) => { base = base.apply(arg); },
                    }
                }
                Some(base)
            },
        } // match next_atom
    }
}
