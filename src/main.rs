use std::collections::HashMap;
use std::env::{self, Args};
use std::fs::File;
use std::io::{self, Read, Write};
use std::iter::Peekable;

use ariadne::Source;

mod builtin;
mod error;
mod interp;
mod parse;
mod scope;
mod types;

#[cfg(test)]
mod tests;

use crate::builtin::NAMES;
use crate::error::SourceRegistry;
use crate::parse::Tree;
use crate::scope::Scope;
use crate::types::{FrozenType, TypeList};

#[derive(Default)]
struct Options {
    do_lookup: bool,
    do_typeof: bool,
    do_repl: bool,
}

struct Global {
    registry: SourceRegistry,
    scope: Scope,
}

fn main() {
    let mut args = env::args().peekable();

    let Some(opts) = Options::parse_args(&mut args) else {
        return;
    };

    let mut global = {
        let mut registry = SourceRegistry::new();

        let source = registry.add_bytes("<prelude>", include_bytes!("prelude.sel").into());
        let scope = parse::process(source, &mut registry, None).scope;

        Global { registry, scope }
    };

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
           -l [<name>...]"
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

    let result = parse::process(
        global.registry.add_bytes(name, bytes),
        &mut global.registry,
        Some(&global.scope),
    );

    if !result.errors.is_empty() {
        // NOTE: err reporting will surely be misleading for some inputs (eg. misplaced indicators)
        let cache = Source::from(String::from_utf8_lossy(global.registry.get_bytes(1)));
        let mut n = 0;
        for e in result.errors {
            e.pretty().eprint(cache.clone()).unwrap();
            n += 1;
        }
        eprintln!("({n} error{})", if 1 == n { "" } else { "s" });
        if let Some(name) = was_file {
            if used_file {
                eprintln!("Note: {name} was interpreted as file name");
            } else {
                eprintln!("Note: {name} is also a file, but it did not start with '#!'");
            }
        }
    }

    result.tree
}

fn do_lookup(mut args: Peekable<Args>, global: Global) {
    match args.peek() {
        None => {
            let mut types = TypeList::default();
            let mut entries: Vec<_> = global.scope.iter_rec().collect();
            entries.sort_unstable_by_key(|p| p.0);
            for (name, val) in entries {
                types.clear();
                let ty = val.make_type(&mut types);
                println!("{name} :: {}", types.frozen(ty));
            }
        }

        Some(oftype) if "::" == oftype => todo!(),

        _ => {
            let mut types = TypeList::default();
            let not_found: Vec<_> = args
                .filter(|name| {
                    if let Some(val) = global.scope.lookup(name) {
                        types.clear();
                        let ty = val.make_type(&mut types);
                        let desc = val.get_desc().unwrap();
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

fn do_repl(global: Global) {
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
                let mut reg = SourceRegistry::new();
                let src = reg.add_bytes("<input>", line.clone().into_bytes());
                let res = parse::process(src, &mut reg, Some(&global.scope));
                if !res.errors.is_empty() {
                    res.errors
                        .into_iter()
                        .try_for_each(|e| e.pretty().eprint(Source::from(&line)))
                        .unwrap();
                } else if let Some((ty, tree)) = res.tree {
                    do_the_thing(&ty, &tree, &global);
                }
                line.clear();
            }
        }
    }
}

fn do_the_thing(ty: &FrozenType, tree: &Tree, global: &Global) {
    use FrozenType::*;
    interp::run_print(interp::interp(tree, &HashMap::new()));
    match ty {
        Number | Pair(_, _) => println!(),
        Bytes(_) | List(_, _) => (),
        Func(_, _) | Named(_) => unreachable!(),
    }
}
