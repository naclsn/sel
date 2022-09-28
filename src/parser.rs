use std::{str::Chars, iter::Peekable};
use crate::engine::Value;

#[derive(Debug, Clone)]
pub enum Binop {
    Composition,
    Application,
    Addition,
    Substraction,
    Multiplication,
    Division,
    Range,
}

#[derive(Debug, Clone)]
pub enum Unop {
    ArrayParse,
}

#[derive(Debug, Clone)]
pub enum Operator {
    Binary(Binop),
    Unary(Unop),
}

#[derive(Debug, Clone)]
pub enum Token {
    Name(String),
    Literal(Value),
    Operator(Operator),
}

pub(crate) struct Lexer<'a> {
    source: Peekable<Chars<'a>>,
    hold: Option<char>,
}

impl<'a> Iterator for Lexer<'a> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        let mut acc: String = "".to_string();
        match self.hold {
            None => None,

            Some(c) if c.is_ascii_digit() => {
                acc.push(c);
                loop {
                    match self.source.next() {
                        Some(cc) if cc.is_ascii_digit() => {
                            acc.push(cc);
                            continue;
                        }
                        may => {
                            self.hold = may;
                            break;
                        }
                    }
                }
                Some(Token::Literal(Value::Num(acc.parse().unwrap())))
            },

            Some(c) if c.is_ascii_lowercase() => {
                acc.push(c);
                loop {
                    match self.source.next() {
                        Some(cc) if cc.is_ascii_lowercase() => {
                            acc.push(cc);
                            continue;
                        }
                        may => {
                            self.hold = may;
                            break;
                        }
                    }
                }
                // let err = &format!("name not in prelude: {acc}");
                // Some(Token::Fit(lookup_prelude(acc).expect(err)))
                Some(Token::Name(acc))
            },

            Some('{') => {
                acc = self.source
                    .by_ref()
                    .take_while(|cc| '}' != *cc)
                    .collect();
                self.hold = self.source.next();
                Some(Token::Literal(Value::Str(acc)))
            },

            Some('\t' | '\r' | '\n' | ' ') => {
                self.hold = self.source
                    .by_ref()
                    .skip_while(|cc| cc.is_ascii_whitespace())
                    .next();
                Some(Token::Operator(Operator::Binary(Binop::Composition)))
            },
            Some(c) => {
                let r = match c {
                    '#' => Some(Token::Operator(Operator::Binary(Binop::Application))),
                    '+' => Some(Token::Operator(Operator::Binary(Binop::Addition))),
                    '-' => Some(Token::Operator(Operator::Binary(Binop::Substraction))),
                    '.' => Some(Token::Operator(Operator::Binary(Binop::Multiplication))),
                    '/' => Some(Token::Operator(Operator::Binary(Binop::Division))),
                    ':' => Some(Token::Operator(Operator::Binary(Binop::Range))),

                    '@' => Some(Token::Operator(Operator::Unary(Unop::ArrayParse))),

                    c => panic!("unhandled character {c}"),
                };
                self.hold = self.source.next();
                r
            }

        } // match hold
    } // fn next
} // impl Iterator for Lexer

pub(crate) fn lex_string<'a>(script: &'a String) -> Lexer<'a> {
    let mut iterator = script.chars().peekable();
    let first = iterator.next();
    Lexer {
        source: iterator,
        hold: first,
    }
}
