use std::{str::Chars, iter::Peekable};
use crate::engine::Value;

#[derive(Debug, Clone)]
pub(crate) enum Binop {
    Composition,
    Addition,
    Substraction,
    Multiplication,
    Division,
    Range,
}

#[derive(Debug, Clone)]
pub(crate) enum Unop {
    Array,
    Flip,
}

#[derive(Debug, Clone)]
pub(crate) enum Operator {
    Binary(Binop),
    Unary(Unop),
}

#[derive(Debug, Clone)]
pub(crate) enum Token {
    Name(String),
    Literal(Value),
    Operator(Operator),
    Grouping(Vec<Token>),
}

pub(crate) struct Lexer<'a> {
    source: Peekable<Chars<'a>>,
}

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

// can I make it take an abstract iterator yielding chars?
pub(crate) fn lex_string<'a>(script: &'a String) -> Lexer<'a> {
    Lexer { source: script.chars().peekable(), }
}
