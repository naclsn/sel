use std::{iter::Peekable, str::Chars, vec};

use crate::{
    engine::{Function, Value},
    prelude::PreludeLookup,
};

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
    // Range,
}

#[derive(Debug, Clone, Copy)]
pub enum Operator {
    Unary(Unop),
    Binary(Binop),
}

#[derive(Debug, Clone)]
pub enum Token {
    Name(String),
    Literal(Value),
    Operator(Operator),
    // SubScript(Vec<Token>),
    SubscriptBegin,
    SubscriptEnd,
    Composition,
}

pub struct Lexer<'a>(Peekable<Chars<'a>>);
fn lex_string<'a>(script: &'a String) -> Lexer<'a> {
    Lexer(script.chars().peekable())
}

impl<'a> Iterator for Lexer<'a> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        match self.0.peek() {
            None => None,

            Some(c) if c.is_ascii_whitespace() => {
                // self.0.next();
                while !self.0.next_if(|cc| cc.is_ascii_whitespace()).is_none() {}
                self.next()
            }

            Some('#') => {
                self.0.by_ref().skip_while(|cc| '\n' != *cc).next();
                self.next()
            }

            Some(c) if c.is_ascii_digit() => {
                let mut acc: String = "".to_string();
                loop {
                    match self.0.next_if(|cc| cc.is_ascii_digit()) {
                        Some(cc) => {
                            acc.push(cc);
                        }
                        None => {
                            break;
                        }
                    }
                }
                Some(Token::Literal(Value::Num(acc.parse().unwrap())))
            }

            Some(c) if c.is_ascii_lowercase() => {
                let mut acc: String = "".to_string();
                loop {
                    match self.0.next_if(|cc| cc.is_ascii_lowercase()) {
                        Some(cc) => {
                            acc.push(cc);
                        }
                        None => {
                            break;
                        }
                    }
                }
                Some(Token::Name(acc))
            }

            Some('{') => {
                let mut lvl: u32 = 0;
                let acc = self
                    .0
                    .by_ref()
                    .take_while(|cc| match cc {
                        '{' => {
                            lvl += 1;
                            true
                        }
                        '}' => {
                            assert!(0 < lvl, "Unbalanced string literal: missing opening '{{'");
                            lvl -= 1;
                            0 < lvl
                        }
                        _ => true,
                    })
                    .skip(1)
                    .collect();
                assert!(0 == lvl, "Unbalanced string literal: missing closing '}}'");
                Some(Token::Literal(Value::Str(acc)))
            }

            Some(c) => {
                let r = match c {
                    '[' => Some(Token::SubscriptBegin),
                    ']' => Some(Token::SubscriptEnd),
                    ',' => Some(Token::Composition),
                    '+' => Some(Token::Operator(Operator::Binary(Binop::Addition))),
                    '-' => Some(Token::Operator(Operator::Binary(Binop::Substraction))),
                    '.' => Some(Token::Operator(Operator::Binary(Binop::Multiplication))),
                    '/' => Some(Token::Operator(Operator::Binary(Binop::Division))),
                    // ':' => Some(Token::Operator(Operator::Binary(Binop::Range))),
                    '@' => Some(Token::Operator(Operator::Unary(Unop::Array))),
                    '%' => Some(Token::Operator(Operator::Unary(Unop::Flip))),
                    '}' => panic!("Unbalanced string literal: missing opening '{{'"),
                    c => todo!("Unhandled character '{c}'"),
                };
                self.0.next();
                r
            }
        } // match peek
    } // fn next
} // impl Iterator for Lexer

pub trait Lex {
    // YYY: may seal it, as well as the Lexer struct
    type TokenIter: Iterator<Item = Token>;
    fn lex(self) -> Self::TokenIter;
}

impl<'a> Lex for &'a String {
    type TokenIter = Lexer<'a>;
    fn lex(self) -> Self::TokenIter {
        lex_string(self)
    }
}
impl Lex for Vec<Token> {
    type TokenIter = vec::IntoIter<Token>;
    fn lex(self) -> Self::TokenIter {
        self.into_iter()
    }
}

#[derive(Clone)]
pub struct Parser<'a, T, L>(Peekable<T::TokenIter>, &'a L)
where
    T: Lex,
    L: PreludeLookup;

pub fn parse_string<'a, L>(script: &'a String, prelude: &'a L) -> Parser<'a, &'a String, L>
where
    L: PreludeLookup,
{
    Parser(script.lex().peekable(), prelude)
}
fn parse_vec<'a, L>(tokens: Vec<Token>, prelude: &'a L) -> Parser<'a, Vec<Token>, L>
where
    L: PreludeLookup,
{
    Parser(tokens.lex().peekable(), prelude)
}

impl FromIterator<Value> for Application {
    fn from_iter<T: IntoIterator<Item = Value>>(iter: T) -> Self {
        Application {
            funcs: iter
                .into_iter()
                .map(|v| match v {
                    Value::Fun(f) => f,
                    other => {
                        println!("trying to build an application from functions,");
                        println!("but got: {other}");
                        panic!("type error");
                    }
                })
                .collect(),
            env: (),
        }
    }
}

impl<'a, T, L> Parser<'a, T, L>
where
    T: Lex,
    L: PreludeLookup,
{
    fn as_value(self) -> Value {
        let fs: Vec<Value> = self.collect();

        if 1 == fs.len() {
            return fs.into_iter().next().unwrap();
        }

        let firstin = fs
            .first()
            .map(|f| match f {
                Value::Fun(f) => f.maps.0.clone(),
                _ => panic!("type error"),
            })
            .unwrap();
        let lastout = fs
            .last()
            .map(|f| match f {
                Value::Fun(f) => f.maps.1.clone(),
                _ => panic!("type error"),
            })
            .unwrap();

        Value::Fun(Function {
            name: "(script)".to_string(),
            maps: (firstin, lastout),
            args: fs,
            func: |this| {
                let arg = this.args.last().unwrap().clone();
                this.args[..this.args.len() - 1]
                    .iter()
                    .fold(arg, |r, f| Function::from(f.clone()).apply(r))
            },
        })
    } // fn result

    /// atom ::= literal
    ///        | name
    ///        | unop (binop | atom)
    ///        | binop atom
    ///        | subscript
    /// subscript ::= '[' [atom binop atom | _elements1] ']'
    fn next_atom(&mut self) -> Option<Value> {
        match self.0.next() {
            None | Some(Token::Composition) => None,

            Some(Token::Literal(value)) => Some(value.clone()),

            Some(Token::Name(name)) => {
                Some(self.1.lookup_name(name.to_string()).expect("Unknown name"))
            }

            Some(Token::Operator(Operator::Unary(un))) => Some(self.1.lookup_unary(un).apply(
                if let Some(Token::Operator(Operator::Binary(bin))) = self.0.peek() {
                    self.1.lookup_binary(*bin).into()
                } else {
                    self.next_atom().expect("Missing argument for unary")
                },
            )),

            Some(Token::Operator(Operator::Binary(bin))) => Some(
                self.1
                    .lookup_binary(bin)
                    .apply(self.next_atom().expect("Missing argument for binary")),
            ),

            Some(Token::SubscriptBegin) => {
                let mut lvl: u32 = 1;
                let tokens = self
                    .0
                    .by_ref()
                    .take_while(|tt| match tt {
                        Token::SubscriptBegin => {
                            lvl += 1;
                            true
                        }
                        Token::SubscriptEnd => {
                            assert!(
                                0 < lvl,
                                "Unbalanced subscript expression: missing opening '['"
                            );
                            lvl -= 1;
                            0 < lvl
                        }
                        _ => true,
                    })
                    .collect();
                assert!(
                    0 == lvl,
                    "Unbalanced subscript expression: missing closing ']'"
                );
                Some(parse_vec(tokens, self.1).as_value())
            }
            Some(Token::SubscriptEnd) => {
                panic!("Unbalanced subscript expression: missing opening '['")
            }
        } // match lexer next
    } // next_atom
} // impl Parser

impl<'a, T, L> Iterator for Parser<'a, T, L>
where
    T: Lex,
    L: PreludeLookup,
{
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
                        None => {
                            break;
                        }
                        Some(arg) => {
                            base = Function::from(base).apply(arg);
                        }
                    }
                }
                Some(base)
            }
        } // match next_atom
    }
}

// YYY: more private fields
pub struct Application {
    pub funcs: Vec<Function>,
    pub env: (),
}

impl Application {
    pub fn apply(&self, line: String) -> String {
        self.funcs
            .iter()
            .fold(Value::Str(line), |acc, cur| cur.clone().apply(acc))
            .as_text()
    }
}
