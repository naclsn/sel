//! checking and lowering into an AST

use std::collections::HashMap;
use std::rc::Rc;

use crate::error::{self, Error, ErrorKind};
use crate::fund::Fund;
use crate::module::{Function, Location, Module, ModuleRegistry};
use crate::parse::{Apply, ApplyBase, Pattern, Script, Value};
use crate::types::{Known, Type};

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

    pub fn errors(&self) -> &[Error] {
        &self.errors
    }

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

        if let Some((prefix, name)) = name.split_once('-') {
            match self.module.retrieve_module(prefix, self.registry) {
                None => {
                    todo!("specialize error: rather than just 'missing', have 'no use :: {prefix}'")
                }
                Some(Err(err)) => todo!("cannot load {prefix}: {err:?}"),
                Some(Ok(module)) => {
                    if let Some(def) = module.retrieve_function(name, self.registry) {
                        return (Type::deep_clone(&def.ast.ty), Refers::File(module, def));
                    } else {
                        todo!("specialize error: rather than just 'missing', have 'no {name} in {prefix}'");
                    }
                }
            }
        }

        self.errors
            .push(error::unknown_name(loc.clone(), name.into()));
        let ty = Type::named(name.into(), []);
        self.scopes
            .last_mut()
            .expect("should always have at least top-level fallback scope")
            .insert(name.into(), (loc.clone(), ty.clone()));
        // TODO push something in scope, so we dont get here again from this case
        (ty, Refers::Missing)
    }

    fn apply(&mut self, func: Tree, arg: Tree) -> Tree {
        let ty = match &*func.ty {
            Type::Known(Known::Func(par, ret)) => {
                if let Err(err) = Type::concretize(par, &arg.ty) {
                    self.errors.push(Error(
                        todo!("loc for {err:?} (from apply not applying)"),
                        err,
                    ));
                }
                ret.clone()
            }
            sad => {
                self.errors.push(Error(
                    todo!("loc for {sad:?} (not a function)"),
                    error::not_function(&func.ty),
                ));
                Type::named(format!("ret"), [])
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

        Tree { ty, val }
    }

    pub fn check_script(&mut self, script: &Script) -> Tree {
        let val = self.check_apply(&script.head);

        if matches!(*val.ty, Type::Known(Known::Func(_, _))) {
            // [f, g, h] -> [pipe f g, h] -> pipe [pipe f g] h
            script.tail.iter().fold(val, |acc, (loc_comma, cur)| {
                let then = self.check_apply(cur);
                let pipe = Tree {
                    ty: Fund::Pipe.make_type(),
                    val: TreeVal::Word(Fund::Pipe.to_string(), Refers::Fundamental(Fund::Pipe)),
                };
                let part = self.apply(pipe, acc);
                self.apply(part, then)
            })
        } else {
            // [x, f, g, h] -> h [g [f x]]
            script.tail.iter().fold(val, |acc, (loc_comma, cur)| {
                let func = self.check_apply(cur);
                self.apply(func, acc)
            })
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
                let ty = Type::named(word.clone(), []);
                if let Some((loc_already, ty)) = names.get(word) {
                    self.errors.push(todo!(
                        "already declared in pattern at {loc_already:?} with :: &{ty}"
                    ));
                    ty.clone()
                } else {
                    names.insert(word.clone(), (todo!("loc"), ty.clone()));
                    ty
                }
            }

            Pattern::List {
                loc_open,
                items,
                rest,
                loc_close,
            } => {
                let items: Box<_> = items
                    .iter()
                    .map(|it| self.check_pattern_type(it, names))
                    .collect();

                // i kept the commented code below that refers to check_list_content;
                // i just rewrote Value::List handling in check_value in a way
                // that's more regular: it's always checked as if {...,, {}} ie it
                // always relies on `cons` which will dictate the behavior uniformly
                // however i'm left with that one now which idk how to then

                let ty: Rc<Type> = todo!("ty of Pattern::List\nitem types: {items:?}");
                //let has = self.check_list_content(items);
                //let fin = self.types.finite(rest.is_none());
                //let ty = self.types.list(fin, has);

                if let Some((_loc_comma, loc, word)) = rest {
                    if let Some((loc_already, ty)) = names.get(word) {
                        self.errors.push(todo!(
                            "already declared in pattern at {loc_already:?} with :: &{ty}"
                        ));
                    } else {
                        names.insert(word.clone(), (loc.clone(), ty));
                    }
                }
                ty
            }

            Pattern::Pair {
                fst,
                loc_equal,
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
        pat: &Pattern,
        res: &Value,
        alt: Option<&Value>,
    ) -> (Rc<Type>, Tree, Option<Tree>) {
        let mut names = HashMap::new();
        let param = self.check_pattern_type(pat, &mut names);
        self.scopes.push(names);

        let res = self.check_value(res);

        let alt = alt.map(|alt| {
            assert!(pat.is_refutable());
            let alt = self.check_value(alt);
            if let Result::<(), ErrorKind>::Err(err) = todo!("Type::harmonize(res.ty, alt.ty)") {
                self.errors.push(Error(
                    todo!("loc for {err:?} (from harmonize res&alt)"),
                    err,
                    //ErrorKind::ContextCaused {
                    //    error: Box::new(Error(result.loc.clone(), e)),
                    //    because: ErrorContext::LetFallbackTypeMismatch {
                    //        result_type: snapshot.frozen(res_ty),
                    //        fallback_type: snapshot.frozen(fallback.ty),
                    //    },
                    //},
                ));
            }
            alt
        });

        self.scopes.pop();
        (Type::func(param, res.ty.clone()), res, alt)
    }

    pub fn check_apply(&mut self, apply: &Apply) -> Tree {
        let base = match &apply.base {
            ApplyBase::Binding {
                loc_let,
                pat,
                res,
                alt,
            } => {
                let (ty, res, alt) = self.check_binding_br(pat, &res, alt.as_deref());
                Tree {
                    ty,
                    val: TreeVal::Binding(pat.clone(), Box::new(res), alt.map(Box::new)),
                }
            }
            ApplyBase::Value(val) => self.check_value(val),
        };

        apply.args.iter().fold(base, |acc, cur| {
            let arg = self.check_value(cur);
            self.apply(acc, arg)
        })
    }

    // not used, but kept for now because of the notes;
    // see in check_pattern_type's Pattern::List branch too
    fn check_list_content(&mut self, items: impl IntoIterator<Item = Rc<Type>>) -> Rc<Type> {
        let ty = Type::named("item".into(), []);
        for it in items {
            // so, unless I'm off, I think the way I did rest part for literal lists
            // will not allow eg {inf, fin,, {}} because it would go:
            //      cons fin {} :: fin
            //      cons inf fin :: type error!
            // looking again, I think that was already the case in previous implementation,
            // so now I'm wondering if I should also disallow when no rest for both
            // consistency and removing hunks from `types` (and almost idealy whole `types.rs`)
            //
            // but does that makes sens? isn't that annoying that it won't be possible to eg
            //      {:hello:, -}, unwords
            // but say in this specific case, maybe the miss is to say literal bytes is inf..?
            //if let Err(err) = Type::harmonize(ty, it) {
            //    self.errors
            //        .push(todo!("error::list_type_mismatch(..) for {err:?}"));
            //}
            // constructing always with `cons` seems like the least sane but most regular approach
            // (insane because that' a de-optimisation which will /have/ to be corrected further down the line)
            //
            // cons <fin> [<fin>] :: [<fin>] -- free
            // cons <inf> [<inf>] :: [<inf>] -- free
            // cons <fin> [<inf>] :: [<inf>] -- can never work through type sys only
            // cons <inf> [<fin>] :: [<inf>] -- works if (no-op) coersion [<fin>] -> [<inf>]
        }
        ty
    }

    pub fn check_value(&mut self, value: &Value) -> Tree {
        match value {
            Value::Number { loc, number } => Tree {
                ty: Type::number(),
                val: TreeVal::Number(*number),
            },

            Value::Bytes { loc, bytes } => Tree {
                ty: {
                    let fin = true;
                    Type::bytes(fin)
                },
                val: TreeVal::Bytes(bytes.clone()),
            },

            Value::Word { loc, word } => {
                let (ty, prov) = self.lookup(loc, word);
                Tree {
                    ty,
                    val: TreeVal::Word(word.clone(), prov),
                }
            }

            Value::Subscr {
                loc_open,
                subscr,
                loc_close,
            } => self.check_script(subscr),

            Value::List {
                loc_open,
                items,
                rest,
                loc_close,
            } => {
                let items: Vec<_> = items.iter().map(|it| self.check_apply(it)).collect();

                let rest = if let Some((loc_comma, rest)) = rest {
                    self.check_apply(rest)
                } else {
                    // empty list, as if {...,, {}}
                    Tree {
                        ty: Type::list(true, Type::named("item".into(), [])),
                        val: TreeVal::List,
                    }
                };

                // cons(items[0], .... cons(items[n-1], cons(items[n], rest)) .... )
                // note: reverse error order -- XXX: may not want that
                items.into_iter().rfold(rest, |acc, cur| {
                    let cons = Tree {
                        ty: Fund::Cons.make_type(),
                        val: TreeVal::Word(Fund::Cons.to_string(), Refers::Fundamental(Fund::Cons)),
                    };
                    let part = self.apply(cons, cur);
                    self.apply(part, acc)
                })
            }

            Value::Pair {
                fst,
                loc_equal,
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
