//! checking and lowering into an AST

use std::collections::HashMap;
use std::rc::Rc;

use crate::errors::{self, Error, MismatchAs};
use crate::fund::Fund;
use crate::module::{Function, Location, Module, ModuleRegistry};
use crate::parse::{Apply, ApplyBase, Located, Pattern, Script, Value};
use crate::types::Type;

#[derive(Debug, Clone)]
pub struct Tree {
    pub ty: Rc<Type>,
    //pub loc: Location, // XXX(wip)?
    pub val: TreeVal,
}

#[derive(Debug, Clone)]
pub enum TreeVal {
    Number(f64),
    Bytes(Box<[u8]>),
    Word(String, Refers),
    List, // represents an empty list, as a cons-sequence sentinel (ie nil)
    Pair(Box<Tree>, Box<Tree>),
    Apply(Box<Tree>, Vec<Tree>), // base is 'Word' or 'Binding', or the error list isn't empty
    Binding(Pattern, Box<Tree>, Option<Box<Tree>>), // Some(.) iff `pat.is_refutable()`
                                 //Fundamental(Fund),
}

#[derive(Debug, Clone)]
pub enum Refers {
    Fundamental(Fund),              // eg cons, panic, bytes...
    Binding(Location),              // from an enclosing let binding
    Defined(Rc<Function>),          // from within this file (def)
    File(Rc<Module>, Rc<Function>), // from external (user) file
    //XXX(wip) Builtin(String),   // eg from prelude // maybe 'll just use module named "<prelude>"
    Missing, // it's shoved in the nearest enclosing scope
}

pub struct Checker<'check> {
    registry: &'check ModuleRegistry,
    module: &'check Module,
    scopes: Vec<HashMap<String, (Location, Rc<Type>)>>, // always at least one
    errors: Vec<Error>,
}

impl<'check> Checker<'check> {
    pub fn new(module: &'check Module, registry: &'check ModuleRegistry) -> Self {
        Self {
            registry,
            module,
            scopes: vec![Default::default()], // top-level fallback scope
            errors: Vec::new(),
        }
    }

    #[allow(dead_code)]
    pub fn errors(&self) -> &[Error] {
        &self.errors
    }
    #[allow(dead_code)]
    pub fn boxed_errors(self) -> Box<[Error]> {
        self.errors.into()
    }

    /// lookup order is always:
    /// - fundamentals
    /// - bindings (closest to furthest)
    /// - defineds (latest to earliest in file)
    /// - uses (same order)
    /// - missing
    ///
    /// when None, likely want to call `missing()`
    /// when ToBeDefined, likely want to call `define()`
    fn lookup(&mut self, loc: &Location, name: &str) -> (Rc<Type>, Refers) {
        if let Some(fund) = Fund::try_from_name(name) {
            return (fund.make_type(), Refers::Fundamental(fund));
        }

        if let Some((old_loc, ty)) = self.scopes.iter().rev().find_map(|it| it.get(name)) {
            return (ty.clone(), Refers::Binding(old_loc.clone()));
        }

        if let Some(def) = self.module.retrieve_function(name, self.registry) {
            return (Type::deep_clone(&def.ast.ty), Refers::Defined(def));
        }

        let mut use_prefix_name = None;
        let mut found_use = None;
        let mut io_err = None;
        let mut not_in_mod = false;
        if let Some((prefix, name)) = name.split_once('-') {
            use_prefix_name = Some((prefix, name));
            match self.module.retrieve_module(prefix, self.registry) {
                None => {}
                Some((loc_use, Err(err))) => {
                    found_use = Some(loc_use);
                    io_err = Some(err)
                }
                Some((loc_use, Ok(module))) => {
                    if let Some(def) = module.retrieve_function(name, self.registry) {
                        return (Type::deep_clone(&def.ast.ty), Refers::File(module, def));
                    }
                    found_use = Some(loc_use);
                    not_in_mod = true;
                }
            }
        }

        let ty = Type::named(name.into());
        let err = errors::unknown_name(
            loc.clone(),
            name.into(),
            ty.clone(),
            Fund::NAMES
                .iter()
                .copied()
                .chain(
                    self.scopes
                        .iter()
                        .rev()
                        .flat_map(|it| it.keys().map(|s| s.as_str())),
                )
                .chain(self.module.defs().iter().map(|d| d.name.as_str()))
                .chain(self.module.uses().iter().map(|u| u.prefix.as_str())),
        );
        self.errors
            .push(match (use_prefix_name, found_use, io_err, not_in_mod) {
                (Some((prefix, _)), None, _, _) => {
                    errors::context_no_use_for_prefix(prefix.into(), err)
                }
                (Some((prefix, _)), Some(loc_use), Some(io_err), _) => {
                    errors::context_cannot_load(prefix.into(), loc_use.clone(), io_err, err)
                }
                (Some((prefix, name)), Some(loc_use), _, true) => {
                    errors::context_not_in_module(prefix.into(), loc_use.clone(), name.into(), err)
                }
                _ => err,
            });

        self.scopes
            .last_mut()
            .expect("should always have at least top-level fallback scope")
            .insert(name.into(), (loc.clone(), ty.clone()));
        (ty, Refers::Missing)
    }

    fn apply(
        &mut self,
        func: Tree,
        arg: Tree,
        loc_basemost: Location,
        loc_func: Location,
        loc_arg: Location,
    ) -> (Tree, Option<Error>) {
        let (ty, err) = match &*func.ty {
            Type::Func(par, ret) => {
                let err = Type::concretize(par, &arg.ty)
                    .err()
                    .map(|(want, give, mut as_of)| {
                        as_of.push(MismatchAs::Wanted(par.deep_clone()));
                        errors::type_mismatch(
                            loc_basemost,
                            loc_func,
                            loc_arg,
                            &want,
                            &give,
                            as_of,
                            Some(&func),
                        )
                    });
                (ret.clone(), err)
            }
            sad => {
                let err = Some(errors::not_function(loc_func, loc_basemost, sad, &func));
                (Type::named("ret".into()), err)
            }
        };

        let val = match func.val {
            TreeVal::Apply(base, mut args) => {
                args.push(arg);
                TreeVal::Apply(base, args)
            }
            TreeVal::Word(_, _) | TreeVal::Binding(_, _, _) => {
                TreeVal::Apply(Box::new(func), vec![arg])
            }
            TreeVal::Number(_) | TreeVal::Bytes(_) | TreeVal::List | TreeVal::Pair(_, _) => {
                TreeVal::Apply(Box::new(func), vec![arg])
            }
        };

        (Tree { ty, val }, err)
    }

    pub fn check_script(&mut self, script: &Script) -> Tree {
        let val = self.check_apply(&script.head);
        let loc = script.head.loc();

        if matches!(*val.ty, Type::Func(_, _)) {
            // [f, g, h] -> [pipe f g, h] -> pipe [pipe f g] h
            script
                .tail
                .iter()
                .fold((loc, val), |(loc_prev, acc), (loc_comma, cur)| {
                    let then = self.check_apply(cur);
                    let loc_cur = cur.loc();
                    let pipe = Tree {
                        ty: Fund::Pipe.make_type(),
                        val: TreeVal::Word(Fund::Pipe.to_string(), Refers::Fundamental(Fund::Pipe)),
                    };
                    let (part, err) = self.apply(
                        pipe,
                        acc,
                        Location("TODO".into(), 0..0),
                        loc_comma.clone(),
                        loc_prev,
                    );
                    self.errors.extend(err);
                    let (res, err) = self.apply(
                        part,
                        then,
                        Location("TODO".into(), 0..0),
                        loc_comma.clone(),
                        loc_cur.clone(),
                    );
                    self.errors.extend(err);
                    (loc_cur, res)
                })
                .1
        } else {
            // [x, f, g, h] -> h [g [f x]]
            script
                .tail
                .iter()
                .fold((loc, val), |(loc_prev, acc), (_loc_comma, cur)| {
                    let func = self.check_apply(cur);
                    let loc_cur = cur.loc();
                    let (r, err) = self.apply(
                        func,
                        acc,
                        Location("TODO".into(), 0..0),
                        loc_cur.clone(),
                        loc_prev,
                    );
                    self.errors.extend(err);
                    (loc_cur, r)
                })
                .1
        }
    }

    fn check_pattern_type(
        &mut self,
        pat: &Pattern,
        names: &mut HashMap<String, (Location, Rc<Type>)>,
    ) -> Rc<Type> {
        match pat {
            Pattern::Number { .. } => Type::number(),

            Pattern::Bytes { .. } => {
                let fin = true;
                Type::bytes(fin)
            }

            Pattern::Word { loc, word } => {
                let ty = Type::named(word.clone());

                let loc = loc.clone();
                use std::collections::hash_map::Entry::*;
                match names.entry(word.clone()) {
                    Occupied(old) => {
                        let (loc_already, ty_old) = old.get();
                        self.errors.push(errors::already_declared(
                            word.clone(),
                            loc_already.clone(),
                            ty_old,
                            loc,
                        ));
                        ty_old.clone()
                    }
                    Vacant(niw) => niw.insert((loc, ty)).1.clone(),
                }
            }

            Pattern::List {
                loc_open,
                items,
                rest,
                loc_close: _,
            } => {
                let ty = Type::list(
                    rest.is_none(),
                    items
                        .iter()
                        .map(|it| (it.loc(), self.check_pattern_type(it, names)))
                        .collect::<Vec<_>>()
                        .into_iter()
                        // mby we wanna rfold for consistency, idc
                        .fold(Type::named("item".into()), |acc, (loc, cur)| {
                            if let Err((want, give, mut as_of)) = Type::concretize(&acc, &cur) {
                                as_of.push(MismatchAs::Wanted(acc.clone()));
                                // meh i dont think this an adapted err
                                self.errors.push(errors::type_mismatch(
                                    Location("TODO".into(), 0..0),
                                    loc_open.clone(),
                                    loc,
                                    &want,
                                    &give,
                                    as_of,
                                    None,
                                ));
                            }
                            acc
                        }),
                );

                if let Some(rest) = rest {
                    let loc = rest.loc_name.clone();
                    use std::collections::hash_map::Entry::*;
                    match names.entry(rest.name.clone()) {
                        Occupied(old) => {
                            let (loc_already, ty_old) = old.get();
                            self.errors.push(errors::context_extra_comma_makes_rest(
                                rest.loc_comma.clone(),
                                errors::already_declared(
                                    rest.name.clone(),
                                    loc_already.clone(),
                                    ty_old,
                                    loc,
                                ),
                            ));
                        }
                        Vacant(niw) => _ = niw.insert((loc, ty.clone())),
                    }
                }
                ty
            }

            Pattern::Pair {
                fst,
                loc_equal: _,
                snd,
            } => {
                let fst = self.check_pattern_type(fst, names);
                let snd = self.check_pattern_type(snd, names);
                Type::pair(fst, snd)
            }
        }
    }

    fn check_binding_br(
        &mut self,
        loc_let: &Location,
        pat: &Pattern,
        res: &Value,
        alt: Option<&Value>,
    ) -> (Rc<Type>, Tree, Option<Tree>) {
        let mut names = HashMap::new();
        let param = self.check_pattern_type(pat, &mut names);
        self.scopes.push(names);

        let ress = self.check_value(res);
        let altt = alt.map(|alt| {
            debug_assert!(pat.is_refutable());
            let altt = self.check_value(alt);
            if let Err((want, give, as_of)) = Type::concretize(&ress.ty, &altt.ty) {
                self.errors.push(errors::context_fallback_mismatch(
                    res.loc(),
                    errors::type_mismatch(
                        Location("TODO".into(), 0..0),
                        loc_let.clone(),
                        alt.loc(),
                        &want,
                        &give,
                        as_of,
                        None,
                    ),
                    &ress.ty,
                    &altt.ty,
                ));
            }
            altt
        });

        self.scopes.pop();
        (Type::func(param, ress.ty.clone()), ress, altt)
    }

    pub fn check_apply(&mut self, apply: &Apply) -> Tree {
        let base = match &apply.base {
            ApplyBase::Binding {
                loc_let,
                pat,
                res,
                alt,
            } => {
                let (ty, res, alt) = self.check_binding_br(loc_let, pat, res, alt.as_deref());
                let val = TreeVal::Binding(pat.clone(), Box::new(res), alt.map(Box::new));
                Tree { ty, val }
            }
            ApplyBase::Value(val) => self.check_value(val),
        };

        let loc_base = apply.base.loc();
        apply
            .args
            .iter()
            .fold((loc_base.clone(), base), |(loc_acc, acc), cur| {
                let loc_cur = cur.loc();
                let arg = self.check_value(cur);
                let (r, err) =
                    self.apply(acc, arg, loc_base.clone(), loc_acc.clone(), loc_cur.clone());
                self.errors.extend(err);
                (Location(loc_cur.0, loc_acc.1.start..loc_cur.1.end), r)
            })
            .1
    }

    pub fn check_value(&mut self, value: &Value) -> Tree {
        match value {
            Value::Number { loc: _, number } => Tree {
                ty: Type::number(),
                val: TreeVal::Number(*number),
            },

            Value::Bytes { loc: _, bytes } => Tree {
                ty: {
                    let fin = true;
                    Type::bytes(fin)
                },
                val: TreeVal::Bytes(bytes.clone()),
            },

            Value::Word { loc, word } => {
                let (ty, prov) = self.lookup(loc, word);
                let val = TreeVal::Word(word.clone(), prov);
                Tree { ty, val }
            }

            Value::Subscr {
                loc_open: _,
                subscr,
                loc_close: _,
            } => self.check_script(subscr),

            Value::List {
                loc_open,
                items,
                rest,
                loc_close: _,
            } => {
                let items: Vec<_> = items.iter().map(|it| (it, self.check_apply(it))).collect();

                let (loc_comma, loc_rest, rest) = if let Some(rest) = rest {
                    let restt = self.check_apply(&rest.apply);
                    (Some(&rest.loc_comma), rest.apply.loc(), restt)
                } else {
                    // empty list, as if {...,, {}}
                    let nil = Tree {
                        ty: Type::list(true, Type::named("item".into())),
                        val: TreeVal::List,
                    };
                    // with Type::list of nil, first apply of snoc cannot fail (:: [a] -> a -> [a])
                    // so use this fake location (ever so slightly cheaper idk)
                    let loc_fake = Location(Default::default(), 0..0);
                    (None, loc_fake, nil)
                };

                // cons(items[0], .... cons(items[n-1], cons(items[n], rest)) .... )
                // snoc(.... snoc(snoc(rest, items[n]), items[n-1]) .... , items[0])
                // note: reverse error order -- XXX: may not want that
                // TODO: if error anyways might want to grab some context to see if we can find the
                //       type that it should be; note: this might just be insane.. at least a
                //       ContextCaused "lists are checked from the end" and group all type err in
                let first = items.len() - 1; // (Rfold, so last is first!)
                items
                    .into_iter()
                    .enumerate()
                    .rfold(rest, |acc, (k, (cur, val))| {
                        let snoc = Tree {
                            ty: Fund::Snoc.make_type(),
                            val: TreeVal::Word(
                                Fund::Snoc.to_string(),
                                Refers::Fundamental(Fund::Snoc),
                            ),
                        };
                        let (part, err) = self.apply(
                            snoc,
                            acc,
                            Location("TODO".into(), 0..0),
                            loc_open.clone(),
                            loc_rest.clone(),
                        );
                        // the first apply can only fail on the first iteration when `acc` (=`rest`) may not be a list
                        if first == k {
                            if let Some(err) = err {
                                self.errors.push(errors::context_extra_comma_makes_rest(
                                    loc_comma.unwrap().clone(),
                                    err,
                                ));
                            }
                        } else {
                            debug_assert!(err.is_none());
                        }
                        let (r, err) = self.apply(
                            part,
                            val,
                            Location("TODO".into(), 0..0),
                            loc_open.clone(),
                            cur.loc(),
                        );
                        self.errors.extend(err);
                        r
                    })
            }

            Value::Pair {
                fst,
                loc_equal: _,
                snd,
            } => {
                let fst = Box::new(self.check_value(fst));
                let snd = Box::new(self.check_value(snd));
                Tree {
                    ty: Type::pair(fst.ty.clone(), snd.ty.clone()),
                    val: TreeVal::Pair(fst, snd),
                }
            }
        }
    }
}

#[test]
fn test() {
    use insta::assert_debug_snapshot;

    fn t(script: &[u8]) -> (Tree, Box<[Error]>) {
        let registry = ModuleRegistry::default();
        let module = registry.load_bytes("<test>", script.iter().copied());
        let function = module.retrieve(&registry).unwrap();
        (function.ast, function.errors)
    }

    assert_debug_snapshot!(t(b"add 1 2"));
    assert_debug_snapshot!(t(b"{1, 2, 3}"));
    assert_debug_snapshot!(t(b"{1, 2, 3, :soleil:}"));
    assert_debug_snapshot!(t(b"{1, 2, 3,, r}"));
    assert_debug_snapshot!(t(b"{tostr, tonum}"));
    assert_debug_snapshot!(t(b"{{tostr}, {tonum}}"));
    assert_debug_snapshot!(t(b"let {a, b, c,, {0}} 1 2"));
    assert_debug_snapshot!(t(b"add 3 2 1"));
    assert_debug_snapshot!(t(b"let a 1 2 3"));
    assert_debug_snapshot!(t(b"add :dda:"));
    assert_debug_snapshot!(t(b"add 1 :dda:"));
    assert_debug_snapshot!(t(b"let {1, r} [r 1] 2"));
    assert_debug_snapshot!(t(b"    prefix-name"));
}
