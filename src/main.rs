use std::env;

mod builtin;
mod error;
mod interp;
mod parse;
mod types;

use crate::parse::Tree;

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
                args.next();
            }
            "-t" => {
                arg_typeof = true;
                args.next();
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

    let (ty, tree) = match Tree::new_typed(args.flat_map(|mut a| {
        a.push(' ');
        a.into_bytes()
    })) {
        Ok(app) => app,
        Err(err) => {
            err.crud_report();
            return;
        }
    };

    if arg_typeof {
        println!("{ty} ## {tree}");
        return;
    }

    interp::run_print(interp::interp(&tree));
}
