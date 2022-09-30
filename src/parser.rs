use std::{str::Chars, iter::Peekable};
use crate::engine::{Value, Apply, Fit};

#[derive(Debug, Clone, PartialEq)]
enum Binop {
    Composition,
    Addition,
    Substraction,
    Multiplication,
    Division,
    Range,
}

#[derive(Debug, Clone, PartialEq)]
enum Unop {
    Array,
    Flip,
}

#[derive(Debug, Clone, PartialEq)]
enum Operator {
    Binary(Binop),
    Unary(Unop),
}

#[derive(Debug, Clone, PartialEq)]
enum Token {
    Name(String),
    Literal(Value),
    Operator(Operator),
    Grouping(Vec<Token>),
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
                Some(Token::Grouping(vec))
            },

            Some(c) => {
                let r = match c {
                    ',' => Some(Token::Operator(Operator::Binary(Binop::Composition))),
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

        } // match hold
    } // fn next
} // impl Iterator for Lexer

pub(crate) struct Parser<'a> { lexer: Peekable<Lexer<'a>> }
pub(crate) fn parse_string<'a>(script: &'a String) -> Parser<'a> { Parser { lexer: lex_string(script).peekable() } }

impl Parser<'_> {
    fn parse_application(&mut self) -> Apply {
        let base = self.parse_base();
        let args: Vec<Value> = vec![];

        loop {
            match self.lexer.peek() {
                None | Some(Token::Operator(Operator::Binary(Binop::Composition))) => { break; },
                _ => { args.push(self.parse_argument()); },
            }
        }

        Apply { base, args }
    }

    fn parse_expression(&mut self) -> Atom {}

    fn parse_base(&mut self) -> Fit {
        todo!()
    }

    fn parse_argument(&mut self) -> Value {
        todo!()
    }
} // impl Parser

impl<'a> Iterator for Parser<'a> {
    type Item = Apply;

    fn next(&mut self) -> Option<Self::Item> {
        self.lexer.peek().map(|_| self.parse_application())
    } // fn next
} // impl Iterator for Parser
