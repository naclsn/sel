use std::{env, io::stdin};

use engine::{Typed, Type};

use crate::{parser::parse_string, engine::{Value, Apply}};

mod parser;
mod engine;
mod prelude;
// mod dsl_playground;

fn main() {
    let app = parse_string(&env::args()
            .skip(1)
            .collect::<Vec<String>>()
            .join(" "))
        .result();

    for line in stdin().lines() {
        match line {
            Ok(it) => {
                let res = app
                    .clone()
                    .apply(Value::Str(it));
                println!("{}", res
                    .clone()
                    .coerse(Type::Str)
                    .unwrap_or(res))
            },
            Err(e) => panic!("{}", e),
        }
    }
}
