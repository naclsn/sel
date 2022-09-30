mod parser;
mod prelude;
mod engine;

use std::{env, io::{self, BufRead}};
use parser::parse_string;

struct Arguments {
    script: String,
}

fn command_args() -> Arguments {
    let mut args = env::args().skip(1).peekable();

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

    Arguments {
        script: args
            .collect::<Vec<String>>()
            .join(" ")
    }
}

fn input() -> impl Iterator<Item=String> {
    io::stdin()
        .lock()
        .lines()
        .map(|it| it.unwrap())
}

fn main() {
    let args = command_args();
    let parser = parse_string(&args.script);

    let lines = input();
    for line in lines {
        println!("apply({line:?})");
    }

}
