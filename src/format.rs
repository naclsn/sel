use std::fmt::{Display, Formatter, Result as FmtResult};

pub fn display_bytes(f: &mut Formatter, bytes: &[u8]) -> FmtResult {
    write!(f, ":")?;
    let mut h = bytes;
    while let Some(c) = h.iter().position(|b| b':' == *b) {
        write!(f, "{}::", String::from_utf8_lossy(&h[..c]))?;
        h = &h[c..];
    }
    write!(f, "{}:", String::from_utf8_lossy(h))
}

/*
use crate::parse::{ApplyBase, Pattern, Tree, TreeKind};

impl Tree {
    pub fn print_simple_ast(&self) {
        use TreeKind::*;

        struct State {
            indent: usize,
            needs_subscr: bool,
        }

        fn print_bytes(bytes: &[u8]) {
            print!(":");
            let mut vv = bytes;
            while let Some(c) = vv.iter().position(|b| b':' == *b) {
                print!("{}::", String::from_utf8_lossy(&vv[..c]));
                vv = &vv[c..];
            }
            print!("{}:", String::from_utf8_lossy(vv));
        }

        fn print_tree(tree: &Tree, state: &mut State) {
            fn is_many_same_line(many: &[Tree]) -> bool {
                many.iter().all(|it| match &it.value {
                    Number(_) => true,
                    Bytes(bytes) if bytes.len() < 12 => true,
                    Apply(Applicable::Name(_), args) if args.is_empty() => true,
                    _ => false,
                })
            }

            match &tree.value {
                Number(n) => print!("{n}"),

                Bytes(bytes) => print_bytes(bytes),

                List(items) => {
                    print!("{{");
                    if !items.is_empty() {
                        if is_many_same_line(items) {
                            print_tree(&items[0], state);
                            for it in &items[1..] {
                                print!(", ");
                                print_tree(it, state);
                            }
                        } else {
                            state.indent += 1;
                            let p_needs_subscr = state.needs_subscr;
                            state.needs_subscr = true;
                            print!(" ");
                            for it in &items[..items.len() - 1] {
                                print_tree(it, state);
                                print!("\n{:1$}, ", "", state.indent * 2 - 2);
                            }
                            print_tree(&items[items.len() - 1], state);
                            print!("\n{:1$}", "", state.indent * 2 - 2);
                            state.indent -= 1;
                            state.needs_subscr = p_needs_subscr;
                        }
                    }
                    print!("}}");
                }

                Apply(base, args) => match base {
                    Applicable::Name(name) => {
                        if state.needs_subscr && !args.is_empty() {
                            print!("[");
                        }
                        print!("{name}");
                        if args.is_empty() || is_many_same_line(args) {
                            for it in args {
                                print!(" ");
                                print_tree(it, state);
                            }
                        } else {
                            state.indent += 1;
                            let p_needs_subscr = state.needs_subscr;
                            state.needs_subscr = true;
                            for it in args {
                                print!("\n{:1$}", "", state.indent * 2);
                                print_tree(it, state);
                            }
                            state.indent -= 1;
                            state.needs_subscr = p_needs_subscr;
                        }
                        if state.needs_subscr && !args.is_empty() {
                            print!("]");
                        }
                    }

                    Applicable::Bind(pattern, result, fallback) => {
                        print!("[let ");
                        print_pattern(pattern);
                        let p_needs_subscr = state.needs_subscr;
                        state.needs_subscr = true;
                        if pattern.is_irrefutable() {
                            print!(" ");
                            print_tree(result, state);
                        } else {
                            state.indent += 1;
                            print!("\n{:1$}", "", state.indent * 2);
                            print_tree(result, state);
                            print!("\n{:1$}", "", state.indent * 2);
                            print_tree(fallback, state);
                            state.indent -= 1;
                        }
                        state.needs_subscr = p_needs_subscr;
                        print!("]");
                    }
                },

                Pair(first, second) => {
                    state.needs_subscr = true;
                    print_tree(first, state);
                    print!("=");
                    print_tree(second, state);
                    state.needs_subscr = false;
                }
            }
        }

        fn print_pattern(pattern: &Pattern) {
            match pattern {
                Pattern::Number(n) => print!("{n}"),
                Pattern::Bytes(bytes) => print_bytes(bytes),
                Pattern::List(items, rest) => {
                    print!("{{");
                    if !items.is_empty() {
                        print_pattern(&items[0]);
                        for it in &items[1..] {
                            print!(", ");
                            print_pattern(it);
                        }
                        if let Some((_, name)) = rest {
                            print!(",, {name}");
                        }
                    }
                    print!("}}");
                }
                Pattern::Name(_, name) => print!("{name}"),
                Pattern::Pair(first, last) => {
                    print_pattern(first);
                    print!("=");
                    print_pattern(last);
                }
            }
        }

        print_tree(
            self,
            &mut State {
                indent: 0,
                needs_subscr: false,
            },
        );
        println!();
    }
}
*/
