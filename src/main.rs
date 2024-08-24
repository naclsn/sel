use std::env::{self, Args};
use std::iter::Peekable;

use ariadne::Source;

mod builtin;
mod error;
mod interp;
mod parse;
mod types;

#[cfg(test)]
mod tests;

use crate::builtin::NAMES;
use crate::parse::Tree;
use crate::types::{FrozenType, TypeList};

struct Options {
    do_lookup: bool,
    do_typeof: bool,
    err_json: bool,
}

fn main() {
    let mut args = env::args().peekable();

    let Some(opts) = Options::parse_args(&mut args) else {
        return;
    };

    if opts.do_lookup {
        do_lookup(args);
        return;
    }

    let mut source = String::new();
    let (ty, tree) = match Tree::new_typed(args.flat_map(|mut a| {
        a.push(' ');
        source += &a;
        a.into_bytes()
    })) {
        Ok(app) => app,
        Err(err) => {
            if opts.err_json {
                let mut json = String::new();
                err.json(&mut json).unwrap();
                println!("{json}");
            } else {
                let mut n = 0;
                for e in err {
                    e.pretty().eprint(Source::from(&source)).unwrap();
                    n += 1;
                }
                eprintln!("({n} error{})", if 1 == n { "" } else { "s" });
            }
            return;
        }
    };

    if opts.do_typeof {
        println!("{ty} ## {tree}");
        return;
    }

    interp::run_print(interp::interp(&tree));
    match ty {
        FrozenType::Number | FrozenType::Pair(_, _) => println!(),
        FrozenType::Bytes(_) | FrozenType::List(_, _) => (),
        FrozenType::Func(_, _) | FrozenType::Named(_) => unreachable!(),
    }
}

impl Options {
    fn parse_args(args: &mut Peekable<Args>) -> Option<Options> {
        let prog = args.next().unwrap();
        let mut r = Options {
            do_lookup: false,
            do_typeof: false,
            err_json: false,
        };

        if let Some(arg) = args.peek() {
            match arg.as_str() {
                "-h" => {
                    eprintln!("Usage: {prog} -h | [-t] <script...> | [-l [<name>...]]");
                    return None;
                }

                "-t" => r.do_typeof = true,
                "-l" => r.do_lookup = true,
                "--error-json" => {
                    r.do_typeof = true;
                    r.err_json = true;
                }

                dash if dash.starts_with('-') => {
                    eprintln!("Unexpected '{dash}', see usage with -h");
                    return None;
                }
                _ => return Some(r),
            }
            args.next();
        } else {
            eprintln!("No argument, see usage with -h");
            return None;
        };

        Some(r)
    }
}

fn do_lookup(mut args: Peekable<Args>) {
    match args.peek() {
        None => {
            let mut types = TypeList::default();
            let mut entries: Vec<_> = NAMES.entries().collect();
            entries.sort_unstable_by_key(|p| *p.0);
            for (name, (mkty, _)) in entries {
                types.clear();
                let ty = mkty(&mut types);
                println!("{name} :: {}", types.frozen(ty));
            }
        }

        Some(oftype) if "::" == oftype => todo!(),

        _ => {
            let mut types = TypeList::default();
            let not_found: Vec<_> = args
                .filter(|name| {
                    if let Some((mkty, desc)) = NAMES.get(name) {
                        types.clear();
                        let ty = mkty(&mut types);
                        println!("{name} :: {}\n\t{desc}", types.frozen(ty));
                        false
                    } else {
                        true
                    }
                })
                .collect();
            if !not_found.is_empty() {
                println!("Not found: {}", not_found.join(", "));
            }
        }
    }
}
