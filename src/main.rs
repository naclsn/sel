use std::env;

mod parser;
mod prelude;
mod engine;

use crate::parser::lex_string;

fn main() {
    let script_args = env::args().skip(1);
    let script = script_args.collect::<Vec<String>>().join(" ");
    println!("-> _{script}_");
    let lexer = lex_string(&script);
    for it in lexer {
        println!("{it:?}");
    }
}
