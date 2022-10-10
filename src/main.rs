use std::{env, io::stdin};

use crate::{
    engine::Type,
    parser::{parse_string, Application},
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
    let prelude = get_prelude();
    let app: Application = parse_string(&script, &prelude).collect();

    if check_only {
        let in_type = app.funcs.first().unwrap().maps.0.clone();
        let out_type = app.funcs.last().unwrap().maps.1.clone();
        println!(":: {}", Type::Fun(Box::new(in_type), Box::new(out_type)));
        return;
    }

    for line in stdin().lines() {
        match line {
            Ok(it) => {
                let res = app.apply(it);
                println!("{}", res)
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
        Some("-c") => {
            check_only = true;
            args.next();
        }
        Some("-l") => {
            args.skip(1).next().map(lookup).or_else(|| {
                for it in get_prelude().list() {
                    println!("{} :: {}", it.name, it.typedef);
                }
                Some(())
            });
            return;
        }
        _ => (),
    }

    let script = args.collect::<Vec<String>>().join(" ");
    process(script, check_only);
}
