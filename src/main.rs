use std::{env, io::stdin};

use crate::{
    engine::{Apply, Type, Typed, Value},
    parser::parse_string,
    prelude::{get_prelude, PreludeLookup},
};

mod engine;
mod parser;
mod prelude;

fn usage(prog: String) {
    println!("Usage: {prog} [-l <name> | [-c] <script>...]");
}

fn lookup(name: String) {
    if let Some(entry) = get_prelude().lookup(name) {
        println!("{} :: {}\n\t{}", entry.name, entry.typedef, entry.docstr)
    } else {
        println!("this name was not found in the prelude")
    }
}

fn process(script: String, check_only: bool) {
    let prelude = &get_prelude();
    let app = parse_string(&script, prelude).result();

    if check_only {
        return;
    }

    for line in stdin().lines() {
        match line {
            Ok(it) => {
                let res = app.clone().apply(Value::Str(it));
                let ty = res.typed();
                println!("{} :: {}", res.clone().coerse(Type::Str).unwrap_or(res), ty)
            }
            Err(e) => panic!("{}", e),
        }
    }
}

fn main() {
    let mut args = env::args().peekable();
    let prog = args.next().unwrap();

    let mut check_only = false;
    match args.peek().map(|it| it.as_str()) {
        Some("-h") | None => {
            return usage(prog);
        }
        Some("-c") => check_only = true,
        Some("-l") => {
            args.next();
            match args.next() {
                Some(name) => lookup(name),
                None => println!("missing name to lookup in the prelude"),
            }
            return;
        }
        _ => (),
    }

    let script = args.skip(1).collect::<Vec<String>>().join(" ");
    process(script, check_only);
}
