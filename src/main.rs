use std::env::{self, Args};
use std::iter::Peekable;

mod builtin;
mod error;
mod interp;
mod parse;
mod types;

#[cfg(test)]
mod tests;

use crate::builtin::NAMES;
use crate::parse::Tree;
use crate::types::TypeList;

struct Options {
    do_lookup: bool,
    do_typeof: bool,
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

    if opts.do_typeof {
        println!("{ty} ## {tree}");
        return;
    }

    interp::run_print(interp::interp(&tree));
}

impl Options {
    fn parse_args(args: &mut Peekable<Args>) -> Option<Options> {
        let prog = args.next().unwrap();
        let mut r = Options {
            do_lookup: false,
            do_typeof: false,
        };

        if let Some(arg) = args.peek() {
            match arg.as_str() {
                "-h" => {
                    eprintln!("Usage: {prog} -h | [-t] <script...> | [-l [<name>...]]");
                    return None;
                }
                "-t" => {
                    args.next();
                    r.do_typeof = true;
                }
                "-l" => {
                    args.next();
                    r.do_lookup = true;
                }
                dash if dash.starts_with('-') => {
                    eprintln!("Unexpected '{dash}', see usage with -h");
                    return None;
                }
                _ => (),
            }
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
            for (name, (_, mkty)) in entries {
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
                    if let Some((desc, mkty)) = NAMES.get(name) {
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
