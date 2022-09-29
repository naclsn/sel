use std::env;

mod parser;
mod prelude;
mod engine;

use crate::parser::lex_string;

fn main() {
    let mut args = env::args().skip(1).inspect(|it| println!("here with {it}")).peekable();
    loop {
        match args.next_if(|arg| arg.starts_with("--")) {
            Some(arg) => {
                match arg.as_str() {
                    "--help" => todo!("Usage: {} script...",
                        env::current_exe() .unwrap()
                        .file_name() .unwrap()
                        .to_str() .unwrap()
                    ),
                    _ => todo!("Unknown argument '{arg}'"),
                }
            }
            None => { break; }
        }
    }
    let script_args = env::args().skip(1);
    let script = script_args.collect::<Vec<String>>().join(" ");
    println!("-> _{script}_");
    let lexer = lex_string(&script);
    for it in lexer {
        println!("{it:?}");
    }
}
