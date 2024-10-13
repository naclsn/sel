use std::env::{self, Args};
use std::fs::File;
use std::io::{self, Read, Write};
use std::iter::Peekable;

mod builtin;
mod error;
mod interp;
mod parse;
mod scope;
mod types;

#[cfg(test)]
mod tests;

use crate::parse::Tree;
use crate::scope::Global;
use crate::types::FrozenType;

#[derive(Default)]
struct Options {
    do_lookup: bool,
    do_typeof: bool,
    do_repl: bool,
}

fn main() {
    let mut args = env::args().peekable();

    let Some(opts) = Options::parse_args(&mut args) else {
        return;
    };

    let mut global = Global::with_builtin();

    //let prelude = include_bytes!("prelude.sel");
    //let source = global.registry.add_bytes("<prelude>", prelude.into());
    //for (k, v) in parse::process(source, &mut global).scope {
    //    global.scope.declare(k, v);
    //}

    if opts.do_lookup {
        do_lookup(args, global);
        return;
    }

    if opts.do_repl {
        do_repl(global);
        return;
    }

    let Some((ty, tree)) = parse_from_args(args, &mut global) else {
        return;
    };

    if opts.do_typeof {
        println!("{ty}");
        return;
    }

    do_the_thing(&ty, &tree, &global);
}

impl Options {
    fn parse_args(args: &mut Peekable<Args>) -> Option<Options> {
        let prog = args.next().unwrap();
        let mut r = Options::default();

        let mut skip = true;
        match args.peek().map(String::as_str) {
            Some("-h") => {
                eprintln!(
                    "Usage: {prog} -h
           [-t] <file> <args...> | <script...>
           -l [<name>...] | :: <type>"
                );
                return None;
            }

            Some("-t") => r.do_typeof = true,
            Some("-l") => r.do_lookup = true,
            None => r.do_repl = true,

            _ => skip = false,
        }
        if skip {
            args.next();
        }

        Some(r)
    }
}

fn parse_from_args(mut args: Peekable<Args>, global: &mut Global) -> Option<(FrozenType, Tree)> {
    let mut was_file = None;
    let mut used_file = false;

    let mut bytes = vec![0, 0]; // for testing '#!'

    let name = if args
        .peek()
        .and_then(|name| {
            File::open(name)
                .ok()
                .inspect(|_| was_file = Some(name.clone()))
        })
        .and_then(|mut file| file.read_exact(&mut bytes).ok().and(Some(file)))
        .filter(|_| *b"#!" == *bytes)
        .and_then(|mut file| file.read_to_end(&mut bytes).ok())
        .is_some()
    {
        used_file = true;
        args.next().unwrap()
    } else {
        bytes.clear(); // the [0, 0] from definition above
        "<args>".into()
    };

    bytes.extend(args.flat_map(|a| {
        let mut a = a.into_bytes();
        a.push(b' ');
        a
    }));

    let result = parse::process(global.registry.add_bytes(name, bytes), global);

    if !result.errors.is_empty() {
        let mut n = 0;
        for e in result.errors {
            eprintln!("{}", e.report(&global.registry));
            n += 1;
        }
        eprintln!("({n} error{})", if 1 == n { "" } else { "s" });
        if let Some(name) = was_file {
            if used_file {
                eprintln!("Note: {name} was interpreted as a file name");
            } else {
                eprintln!("Note: {name} is also a file, but it did not start with '#!'");
            }
        }
        None
    } else {
        result
            .tree
            .as_ref()
            .map(|t| global.types.frozen(t.ty))
            .zip(result.tree)
    }
}

fn do_lookup(mut args: Peekable<Args>, mut global: Global) {
    match args.peek() {
        None => {
            let mut entries: Vec<_> = global.scope.iter().collect();
            entries.sort_unstable_by_key(|p| p.0);
            for (name, val) in entries {
                // TODO: this accumulates all into types when
                //       it could be reverted after each print
                let ty = val.make_type(&mut global.types);
                println!("{name} :: {}", global.types.frozen(ty));
            }
        }

        Some(oftype) if "::" == oftype => {
            let search = global
                .types
                .parse_str(&args.nth(1).unwrap())
                .unwrap();
            let mut entries: Vec<_> = global
                .scope
                .iter()
                .filter_map(|(name, val)| {
                    // TODO: this accumulates all into types when
                    //       it could be reverted after each print
                    let ty = val.make_type(&mut global.types);
                    // FIXME: wrong
                    if types::Type::compatible(search, ty, &global.types) {
                        Some((name, ty))
                    } else {
                        None
                    }
                })
                .collect();
            entries.sort_unstable_by_key(|p| p.0);
            for (name, ty) in entries {
                println!("{name} :: {}", global.types.frozen(ty));
            }
        }

        _ => {
            let not_found: Vec<_> = args
                .filter(|name| {
                    if let Some(val) = global.scope.lookup(name) {
                        // TODO: this accumulates all into types when
                        //       it could be reverted after each print
                        let ty = val.make_type(&mut global.types);
                        let desc = val.get_desc().unwrap();
                        println!("{name} :: {}\n\t{desc}", global.types.frozen(ty));
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

fn do_repl(mut global: Global) {
    let stdin = io::stdin();
    let mut stdout = io::stdout();

    let mut line = String::new();
    while let Ok(n) = {
        stdout
            .write_all(if line.is_empty() { b">> " } else { b".. " })
            .unwrap();
        stdout.flush().unwrap();
        stdin.read_line(&mut line)
    } {
        match n {
            0 => break,
            1 => {
                line.pop();
                continue;
            }
            _ if b'\\' == line.as_bytes()[n - 2] => {
                line.pop();
                line.pop();
                line.push('\n');
                continue;
            }
            _ => {
                // XXX: adds a new entry with the same name each round,
                //      same with `types` accumulating more than needed
                let src = global
                    .registry
                    .add_bytes("<input>", line.clone().into_bytes());
                let res = parse::process(src, &mut global);
                if !res.errors.is_empty() {
                    for e in res.errors {
                        eprintln!("{}", e.report(&global.registry));
                    }
                } else if let Some(tree) = res.tree {
                    do_the_thing(&global.types.frozen(tree.ty), &tree, &global);
                }
                line.clear();
            }
        }
    }
}

fn do_the_thing(ty: &FrozenType, tree: &Tree, global: &Global) {
    use FrozenType::*;
    interp::run_write(tree, global, &mut io::stdout()).unwrap();
    match ty {
        Number | Pair(_, _) => println!(),
        Bytes(_) | List(_, _) => (),
        Func(_, _) | Named(_) => unreachable!(),
    }
}
