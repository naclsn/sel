use crate::{
    engine::{Apply, Type, Typed, Value},
    parser::parse_string,
};
use std::{env, io::stdin};

mod engine;
mod parser;
mod prelude;
// mod dsl_playground;

fn main() {
    let app = parse_string(&env::args().skip(1).collect::<Vec<String>>().join(" ")).result();

    for line in stdin().lines() {
        match line {
            Ok(it) => {
                let res = app.clone().apply(Value::Str(it));
                println!("{}", res.clone().coerse(Type::Str).unwrap_or(res))
            }
            Err(e) => panic!("{}", e),
        }
    }
}
