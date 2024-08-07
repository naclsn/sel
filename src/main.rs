use std::env;

mod builtin;
mod interp;
mod parse;

use parse::{Error, Tree};

fn main() {
    let mut args = env::args().peekable();
    let prog = args.next().unwrap();

    let mut arg_lookup = false;
    let mut arg_typeof = false;

    if let Some(arg) = args.peek() {
        match arg.as_str() {
            "-h" => {
                eprintln!("Usage: {prog} [-t] <script...> | [-l] <name>...");
                return;
            }
            "-l" => {
                arg_lookup = true;
                drop(args.next());
            }
            "-t" => {
                arg_typeof = true;
                drop(args.next());
            }
            dash if dash.starts_with('-') => {
                eprintln!("Unexpected '{dash}', see usage with -h");
                return;
            }
            _ => (),
        }
    } else {
        eprintln!("No argument, see usage with -h");
        return;
    };

    if arg_lookup {
        todo!("-l: lookup and list names");
    }

    let (ty, types, tree) = match Tree::new_typed(args.flat_map(|mut a| {
        a.push(' ');
        a.into_bytes()
    })) {
        Ok(app) => app,
        Err(e) => {
            match e {
                Error::Unexpected(t) => eprintln!("Unexpected token {t:?}"),
                Error::UnexpectedEnd => eprintln!("Unexpected end of script"),
                Error::UnknownName(n) => eprintln!("Unknown name '{n}'"),
                Error::NotFunc(o, types) => {
                    eprintln!("Expected a function type, but got {}", types.repr(o))
                }
                Error::ExpectedButGot(w, g, types) => {
                    eprintln!("Expected type {}, but got {}", types.repr(w), types.repr(g))
                }
                Error::InfWhereFinExpected => {
                    eprintln!("Expected finite type, but got infinite type")
                }
            }
            return;
        }
    };

    if arg_typeof {
        println!("{} ## {tree}", types.repr(ty));
        return;
    }

    let val = interp::interp(&tree);
    drop(tree);
    interp::run_print(val);
}
