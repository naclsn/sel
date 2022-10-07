use std::{env, io::stdin};

use crate::{
    engine::{Apply, Typed, Value},
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

fn process(script: String, check_only: bool, debug_types: bool) {
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
                if debug_types {
                    println!("{} :: {}", res, ty)
                } else {
                    println!("{}", res.as_text())
                }
            }
            Err(e) => panic!("{}", e),
        }
    }
}

fn main() {
    let mut args = env::args().peekable();
    let prog = args.next().unwrap();

    let mut check_only = false;
    let mut debug_types = false;
    match args.peek().map(|it| it.as_str()) {
        Some("-h") | None => {
            return usage(prog);
        }
        Some("-c") => {
            check_only = true;
            args.next();
        }
        Some("-d") => {
            debug_types = true;
            args.next();
        }
        Some("-l") => {
            args.skip(1)
                .next()
                .map(lookup)
                .or_else(|| Some(println!("missing name to lookup in the prelude")));
            return;
        }
        _ => (),
    }

    let script = args.collect::<Vec<String>>().join(" ");
    process(script, check_only, debug_types);
}
