//! checking and lowering into an AST

use std::collections::HashMap;

use crate::error::{self, Error};
use crate::fund::Fund;
use crate::parse::{Apply, ApplyBase, Pattern, Script, Value};
use crate::scope::{Entry, Location, Scoping, SourceRef};
use crate::types::{Type, TypeList, TypeRef};

#[derive(Debug, Clone)]
pub struct Tree {
    pub ty: TypeRef,
    pub val: TreeVal,
}

#[derive(Debug, Clone)]
pub enum TreeVal {
    Number(f64),
    Bytes(Box<[u8]>),
    Word(String, Refers),
    List(Box<[Tree]>),
    Pair(Box<Tree>, Box<Tree>),
    Apply(Box<Tree>, Vec<Tree>), // base can only be 'Word' or 'Binding'
    Binding(Pattern, Box<Tree>, Box<Tree>), // garbage if `pat.is_irrefutable()`
    Fundamental(Fund),
}

#[derive(Debug, Clone)]
pub enum Refers {
    Binding(Location), // from an enclosing let binding
    File(SourceRef),   // from external (user) file
    Builtin(String),   // eg from prelude
    Fundamental,       // eg cons, panic, bytes...
    Missing,
}

pub struct Checker<'t, 's> {
    types: &'t mut TypeList,
    scope: &'s mut Scoping,
    errors: Vec<Error>,
}

impl<'t, 's> Checker<'t, 's> {
    pub fn new(types: &'t mut TypeList, scope: &'s mut Scoping) -> Self {
        Self {
            types,
            scope,
            errors: Vec::new(),
        }
    }

    pub fn errors(&self) -> &[Error] {
        &self.errors
    }

    fn apply(&mut self, func: Tree, arg: Tree) -> Tree {
        todo!("apply {func:?} {arg:?}")
    }

    fn fundamental(&mut self, op: Fund) -> Tree {
        Tree {
            ty: op.make_type(self.types),
            val: TreeVal::Fundamental(op),
        }
    }

    pub fn check_script(&mut self, script: &Script) -> Tree {
        let val = self.check_apply(&script.head);

        if matches!(self.types.get(val.ty), Type::Func(_, _)) {
            // [f, g, h] -> [pipe f g, h] -> pipe [pipe f g] h
            script.tail.iter().fold(val, |acc, cur| {
                let then = self.check_apply(cur);
                let pipe = self.fundamental(Fund::Pipe);
                let part = self.apply(pipe, acc);
                self.apply(part, then)
            })
        } else {
            // [x, f, g, h] -> h [g [f x]]
            script.tail.iter().fold(val, |acc, cur| {
                let func = self.check_apply(cur);
                self.apply(func, acc)
            })
        }
    }

    fn check_pattern_type(
        &mut self,
        pat: &Pattern,
        names: &mut HashMap<String, (Location, TypeRef)>,
    ) -> TypeRef {
        match pat {
            Pattern::Number(_) => self.types.number(),

            Pattern::Bytes(_) => {
                let fin = self.types.finite(true);
                self.types.bytes(fin)
            }

            Pattern::Word(w) => {
                let ty = self.types.named(w.clone());
                if let Some((loc, ty)) = names.get(w) {
                    self.errors.push(todo!(
                        "already declared in pattern at {loc:?} with :: &{ty}"
                    ));
                    *ty
                } else {
                    names.insert(w.clone(), (todo!("loc"), ty));
                    ty
                }
            }

            Pattern::List(items, rest) => {
                let items: Box<_> = items
                    .iter()
                    .map(|it| self.check_pattern_type(it, names))
                    .collect();

                let has = self.check_list_content(items);
                let fin = self.types.finite(rest.is_none());
                let ty = self.types.list(fin, has);

                if let Some(w) = rest {
                    if let Some((loc, ty)) = names.get(w) {
                        self.errors.push(todo!(
                            "already declared in pattern at {loc:?} with :: &{ty}"
                        ));
                    } else {
                        names.insert(w.clone(), (todo!("loc"), ty));
                    }
                }
                ty
            }

            Pattern::Pair(fst, snd) => {
                let fst = self.check_pattern_type(fst, names);
                let snd = self.check_pattern_type(snd, names);
                self.types.pair(fst, snd)
            }
        }
    }

    fn check_binding_br(
        &mut self,
        pat: &Pattern,
        res: &Value,
        alt: &Value,
    ) -> (TypeRef, Tree, Tree) {
        let mut names = HashMap::new();
        let param = self.check_pattern_type(pat, &mut names);
        self.scope.push(names);

        let res = self.check_value(res);

        let alt = if pat.is_irrefutable() {
            Tree {
                ty: self.types.number(),
                val: TreeVal::Number(0.0),
            }
        } else {
            let alt = self.check_value(alt);
            if let Err(err) = Type::harmonize(res.ty, alt.ty, self.types) {
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
        };

        self.scope.pop();
        (self.types.func(param, res.ty), res, alt)
    }

    pub fn check_apply(&mut self, apply: &Apply) -> Tree {
        let base = match &apply.base {
            ApplyBase::Binding(pat, res, alt) => {
                let (ty, res, alt) = self.check_binding_br(pat, &res, &alt);
                Tree {
                    ty,
                    val: TreeVal::Binding(pat.clone(), Box::new(res), Box::new(alt)),
                }
            }
            ApplyBase::Value(val) => self.check_value(val),
        };

        apply.args.iter().fold(base, |acc, cur| {
            let arg = self.check_value(cur);
            self.apply(acc, arg)
        })
    }

    fn check_list_content(&mut self, items: impl IntoIterator<Item = TypeRef>) -> TypeRef {
        let ty = self.types.named("item".into());
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
            if let Err(err) = Type::harmonize(ty, it, &mut self.types) {
                self.errors
                    .push(todo!("error::list_type_mismatch(..) for {err:?}"));
            }
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
            Value::Number(n) => Tree {
                ty: self.types.number(),
                val: TreeVal::Number(*n),
            },

            Value::Bytes(b) => Tree {
                ty: {
                    let fin = self.types.finite(true);
                    self.types.bytes(fin)
                },
                val: TreeVal::Bytes(b.clone()),
            },

            Value::Word(w) => {
                let (ty, prov) = match self.scope.lookup(w) {
                    Some(Entry::Binding(binding)) => {
                        (binding.ty, Refers::Binding(binding.loc.clone()))
                    }
                    Some(Entry::Defined(defined)) => (
                        self.types.duplicate(defined.ty, &mut Default::default()),
                        Refers::File(defined.loc.0),
                    ),
                    Some(Entry::Fundamental(fund)) => {
                        (fund.make_type(self.types), Refers::Fundamental)
                    }
                    None => {
                        let ty = self.types.named(w.clone());
                        self.scope.missing(w.clone(), ty);
                        (ty, Refers::Missing)
                    }
                };
                Tree {
                    ty,
                    val: TreeVal::Word(w.clone(), prov),
                }
            }

            Value::Subscr(script) => self.check_script(script),

            Value::List(items, rest) => {
                let items: Vec<_> = items.iter().map(|it| self.check_apply(it)).collect();

                if let Some(rest) = rest {
                    let rest = self.check_apply(rest);
                    // cons(items[0], .... cons(items[n-1], cons(items[n], rest)) .... )
                    // note: reverse error order -- do we want not that?
                    items.into_iter().rfold(rest, |acc, cur| {
                        let cons = self.fundamental(Fund::Cons);
                        let part = self.apply(cons, cur);
                        self.apply(part, acc)
                    })
                } else {
                    Tree {
                        ty: {
                            let has = self.check_list_content(items.iter().map(|it| it.ty));
                            let fin = self.types.finite(true);
                            self.types.list(fin, has)
                        },
                        val: TreeVal::List(items.into_boxed_slice()),
                    }
                }
            }

            Value::Pair(fst, snd) => {
                let fst = Box::new(self.check_value(fst));
                let snd = Box::new(self.check_value(snd));
                Tree {
                    ty: self.types.pair(fst.ty, snd.ty),
                    val: TreeVal::Pair(fst, snd),
                }
            }
        }
    }
}
