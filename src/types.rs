use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Peekable};
use std::rc::Rc;

use crate::error::ErrorKind;

pub type TypeRef = Rc<RefCell<Type>>;
pub type BoundRef = Rc<RefCell<Type>>;

#[derive(Debug, Clone)]
pub enum Type {
    Number,
    Bytes(BoundRef),
    List(BoundRef, TypeRef),
    Func(TypeRef, TypeRef),
    Pair(TypeRef, TypeRef),
    Named(String),
}
#[derive(Debug, Clone)]
pub enum Bound {
    Finite(bool),
    FiniteBoth(BoundRef, BoundRef),
    FiniteEither(BoundRef, BoundRef),
}

use Type::*;

#[derive(PartialEq, Debug, Clone)]
pub enum FrozenType {
    Number,
    Bytes(bool),
    List(bool, Box<FrozenType>),
    Func(Box<FrozenType>, Box<FrozenType>),
    Pair(Box<FrozenType>, Box<FrozenType>),
    Named(String),
}

impl Default for TypeList {
    fn default() -> Self {
        let r = TypeList(vec![
            Type::Number,       // [NUMBER_TYPEREF]
            Type::Bytes(2),     // [STRFIN_TYPEREF]
            Type::Finite(true), // [FINITE_TYPEREF]
        ]);
        r.transaction_group("default".into());
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(&r, "default".into());
        r
    }
}

// types vec, also factory and such {{{
impl TypeList {
    pub fn transaction_group(&self, _hi: String) {
        #[cfg(feature = "types-snapshots")]
        types_snapshots::group_dot_lines(_hi);
    }

    fn push(&mut self, it: Type) -> TypeRef {
        self.0.push(it);
        let k = self.0.len() - 1;
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            self,
            format!("push {k} :: {}", types_snapshots::string_any_type(self, k)),
        );
        k
    }

    pub fn get(&self, at: TypeRef) -> &Type {
        match &self.0[at] {
            &Transparent(u) => self.get(u),
            r => r,
        }
    }

    /// can only `set` when `at` is either a named or a tr-chain to one;
    /// more specifically called when:
    ///  * upgrading a func.ty from named to func in `try_apply`
    ///  * matching 2 named (/ tr to named) in `concretize`
    ///  * aliasing named to known in `concretize`
    ///
    /// tr-chains can go more than 1 level because a named could have been changed into
    /// a transparent under our feets (/normally/ there should only be this one call to
    /// `set` in `concretize` with a transparent that can create this situation...
    pub(crate) fn set(&mut self, mut at: TypeRef, ty: Type) {
        // find the actual cell that will be updated
        while let &Transparent(atu) = &self.0[at] {
            at = atu;
        }
        let at = at;
        assert!(
            matches!(self.0[at], Named(_)),
            "assertion failed: should only be assigning to variable types, not constant {:?}",
            self.0[at],
        );

        let ty = match ty {
            Number => Transparent(NUMBER_TYPEREF),
            Bytes(FINITE_TYPEREF) => Transparent(STRFIN_TYPEREF),
            Finite(true) => Transparent(FINITE_TYPEREF),
            Transparent(mut u) => {
                // flatten any tr-chain to single level indirection
                while let &Transparent(uu) = &self.0[u] {
                    u = uu;
                }
                // a `set` can make a (or two) tr-chain(s) into a tr-loop iif both `at` and `ty`
                // are within the same chain (within two chains that ends on the same last thingy),
                // ie the two have the same last thingy at the end, ie `at == u` (after the two
                // `while let` tr resolution loops)
                // likely that the correct behavior is to do nothing about it, ie early return to
                // keep the tr-chain as-is
                // yes it is because `at == u` means we just established that we (caller) are
                // trying to make both `at` and `ty` refer to the same last thingy but they both
                // already do (so job done)
                //assert!(at != u, "assertion failed: set({at}, {ty:?}) will result in a tr-loop");
                if at == u {
                    return;
                }
                Transparent(u)
            }
            _ => ty,
        };

        #[cfg(feature = "types-snapshots")]
        let was = types_snapshots::string_any_type(self, at);

        self.0[at] = ty;

        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            self,
            format!(
                "set {at} :: {} (was {})",
                types_snapshots::string_any_type(self, at),
                was
            ),
        );
    }

    pub fn number(&self) -> TypeRef {
        NUMBER_TYPEREF
    }
    pub fn bytes(&mut self, finite: Bound) -> TypeRef {
        if FINITE_TYPEREF == finite {
            STRFIN_TYPEREF
        } else {
            self.push(Bytes(finite))
        }
    }
    pub fn list(&mut self, finite: Bound, item: TypeRef) -> TypeRef {
        self.push(List(finite, item))
    }
    pub fn func(&mut self, par: TypeRef, ret: TypeRef) -> TypeRef {
        self.push(Func(par, ret))
    }
    pub fn pair(&mut self, fst: TypeRef, snd: TypeRef) -> TypeRef {
        self.push(Pair(fst, snd))
    }
    pub fn named(&mut self, name: String) -> TypeRef {
        self.push(Named(name.to_string()))
    }
    pub fn finite(&mut self, finite: bool) -> Bound {
        if finite {
            FINITE_TYPEREF
        } else {
            self.push(Finite(false))
        }
    }
    pub fn either(&mut self, left: Bound, right: Bound) -> Bound {
        self.push(FiniteEither(left, right))
    }
    pub fn both(&mut self, left: Bound, right: Bound) -> Bound {
        self.push(FiniteBoth(left, right))
    }

    // (ofc does not support pseudo-notations)
    // TODO: could be made into supporting +1, +2.. notation
    pub fn parse_str(&mut self, s: &str) -> Option<TypeRef> {
        let mut nameds = HashMap::new();

        let mut s = s.bytes().peekable();
        let tokens: Vec<_> = iter::from_fn(|| match s.find(|c: &u8| !c.is_ascii_whitespace())? {
            b'-' => s.next().filter(|c| b'>' == *c),

            b'N' => s
                .next()
                .zip(s.next())
                .filter(|p| (b'u', b'm') == *p)
                .map(|_| b'N'),
            b'S' => s
                .next()
                .zip(s.next())
                .filter(|p| (b't', b'r') == *p)
                .map(|_| b'S'),

            c @ (b'(' | b')' | b'+' | b',' | b'[' | b']') => Some(c),

            c @ b'a'..=b'z' => {
                let mut name = vec![c];
                name.extend(iter::from_fn(|| s.next_if(u8::is_ascii_lowercase)));
                let name = unsafe { String::from_utf8_unchecked(name.clone()) };
                nameds.get(&name).copied().or_else(|| {
                    let named = self.named(name.clone()) as u8;
                    nameds.insert(name, named);
                    Some(named)
                })
            }

            _ => None,
        })
        .collect();

        fn _impl(
            types: &mut TypeList,
            tokens: &mut Peekable<impl Iterator<Item = u8>>,
        ) -> Option<TypeRef> {
            let par = match tokens.next()? {
                b'N' => types.number(),
                b'S' => {
                    let fin = types.finite(tokens.next_if(|t| b'+' == *t).is_none());
                    types.bytes(fin)
                }

                b'[' => {
                    let item = _impl(types, tokens)?;
                    tokens.next_if(|t| b']' == *t)?;
                    let fin = types.finite(tokens.next_if(|t| b'+' == *t).is_none());
                    types.list(fin, item)
                }

                b'(' => {
                    let fst = _impl(types, tokens)?;
                    let r = if tokens.next_if(|t| b',' == *t).is_some() {
                        let snd = _impl(types, tokens)?;
                        types.pair(fst, snd)
                    } else {
                        fst
                    };
                    tokens.next_if(|t| b')' == *t)?;
                    r
                }

                k if k < 20 => k as usize,

                _ => return None,
            };

            Some(if tokens.next_if(|t| b'>' == *t).is_some() {
                let ret = _impl(types, tokens)?;
                types.func(par, ret)
            } else {
                par
            })
        }

        _impl(self, &mut tokens.into_iter().peekable())
    }

    // TODO(maybe): fn flatten(&mut);

    /// note: a duplicate is always a tr-free subgraph
    // TODO(maybe): fn duplicate_from(&mut, &, at);
    pub(crate) fn duplicate(
        &mut self,
        at: TypeRef,
        already_done: &mut HashMap<usize, usize>,
    ) -> TypeRef {
        if let Some(done) = already_done.get(&at) {
            return *done;
        }

        // raw array access to prenvent working around transparent
        // (need to account for them in duplication)
        let r = match &self.0[at] {
            &Number => self.number(),
            &Bytes(f) => {
                let f = self.duplicate(f, already_done);
                self.bytes(f)
            }
            &List(f, i) => {
                let (f, i) = (
                    self.duplicate(f, already_done),
                    self.duplicate(i, already_done),
                );
                self.list(f, i)
            }
            &Func(p, r) => {
                let (p, r) = (
                    self.duplicate(p, already_done),
                    self.duplicate(r, already_done),
                );
                self.func(p, r)
            }
            &Pair(f, s) => {
                let (f, s) = (
                    self.duplicate(f, already_done),
                    self.duplicate(s, already_done),
                );
                self.pair(f, s)
            }
            Named(n) => {
                let n = n.clone();
                self.named(n)
            }
            &Finite(i) => self.finite(i),
            &FiniteBoth(l, r) => {
                let (l, r) = (
                    self.duplicate(l, already_done),
                    self.duplicate(r, already_done),
                );
                self.both(l, r)
            }
            &FiniteEither(l, r) => {
                let (l, r) = (
                    self.duplicate(l, already_done),
                    self.duplicate(r, already_done),
                );
                self.either(l, r)
            }
            &Transparent(mut u) => {
                // flatten any tr-chain to single level indirection
                while let &Transparent(uu) = &self.0[u] {
                    u = uu;
                }
                // 2 cases:
                // - the underlying type has already been duplicated => use `already_done`
                // - this is the first access => first duplicate `under`
                // we know this is not getting/duplicating a tr in either branch because:
                // - `already_done` memoizes results from `duplicate` and
                // - `duplicate` only creates tr for a tr and
                // - `u` does not refer to a tr (from the `while` above)
                already_done
                    .get(&u)
                    .copied()
                    .unwrap_or_else(|| self.duplicate(u, already_done))
            }
        };

        already_done.insert(at, r);
        r
    }

    fn freeze_finite(&self, at: Bound) -> bool {
        match *self.get(at) {
            Finite(b) => b,
            FiniteBoth(l, r) => self.freeze_finite(l) && self.freeze_finite(r),
            FiniteEither(l, r) => self.freeze_finite(l) || self.freeze_finite(r),

            Number
            | Bytes(_)
            | List(_, _)
            | Func(_, _)
            | Pair(_, _)
            | Named(_)
            | Transparent(_) => unreachable!(),
        }
    }

    pub fn frozen(&self, at: TypeRef) -> FrozenType {
        match self.get(at) {
            Number => FrozenType::Number,
            Bytes(b) => FrozenType::Bytes(self.freeze_finite(*b)),
            List(b, items) => {
                FrozenType::List(self.freeze_finite(*b), Box::new(self.frozen(*items)))
            }
            Func(par, ret) => {
                FrozenType::Func(Box::new(self.frozen(*par)), Box::new(self.frozen(*ret)))
            }
            Pair(fst, snd) => {
                FrozenType::Pair(Box::new(self.frozen(*fst)), Box::new(self.frozen(*snd)))
            }
            Named(name) => FrozenType::Named(name.clone()),
            Finite(_) | FiniteBoth(_, _) | FiniteEither(_, _) | Transparent(_) => unreachable!(),
        }
    }
}
// }}}

// type manipulation {{{
impl Type {
    /// `func give` so returns type of `func` (if checks out).
    pub(crate) fn applied(
        func: TypeRef,
        give: TypeRef,
        types: &mut TypeList,
    ) -> Result<TypeRef, ErrorKind> {
        match types.get(func) {
            &Func(want, ret) => Type::concretize(want, give, types, false).map(|()| ret),
            _ => Err(ErrorKind::NotFunc {
                actual_type: types.frozen(func),
            }),
        }
    }

    /// Same idea as `applied` but doesn't mutate anything and doesn't fail but returns true/false.
    /// (! see comment on underlying `compatible` itself)
    pub(crate) fn applicable(func: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match types.get(func) {
            &Func(want, _) => Type::compatible(want, give, types),
            _ => false,
        }
    }

    /// Same as `applied` but `curr` already the destination and
    /// handling of finiteness is different.
    /// It's used when finding the item type of a list:
    /// ```plaintext
    /// a | [Num]+ | [Num] ->> [Num]+
    /// a | Str | Num ->> never
    /// a | Str+ | Str ->> Str+
    /// a | Str | Str+ ->> never // xxx: disputable, same with []|[]+
    /// a | [Str+] | [Str] ->> [Str+]
    /// a | (Str -> c) | (Str+ -> Str+) ->> (Str -> Str+)
    /// a | ([Str+] -> x) | ([Str] -> x) ->> ([Str] -> x)
    /// ```
    pub(crate) fn harmonize(
        curr: TypeRef,
        item: TypeRef,
        types: &mut TypeList,
    ) -> Result<(), ErrorKind> {
        Type::concretize(curr, item, types, true)
    }

    /// Concretizes boundedness and unknown named types:
    /// * fin <- fin is fin
    /// * inf <- inf is inf
    /// * inf <- fin is fin
    /// * unk <- Knw is Knw
    ///
    /// Both types may be modified (due to contravariance of func in parameter type).
    /// This uses the fact that inf types and unk named types are
    /// only depended on by the functions that introduced them.
    ///
    /// When called from `harmonize` (keep_inf true):
    /// * inf <- fin is inf
    fn concretize(
        want: TypeRef,
        give: TypeRef,
        types: &mut TypeList,
        keep_inf: bool,
    ) -> Result<(), ErrorKind> {
        fn handle_finiteness(
            keep_inf: bool,
            fw: Bound,
            fg: Bound,
            types: &mut TypeList,
        ) -> Result<(), ErrorKind> {
            match (types.freeze_finite(fw), types.freeze_finite(fg)) {
                (false, false) | (true, true) => Ok(()),
                (false, true) => {
                    if !keep_inf {
                        // intentionally working around `set` to skip unneeded complexity
                        // we know anyways that we are replacing a 'finite' with an other one
                        // this is maybe discussable as it can break 'finite-both/either' links..
                        types.0[fw] = Finite(true);
                        //types.set(fw, Finite(true));
                    }
                    Ok(())
                }
                (true, false) => Err(ErrorKind::InfWhereFinExpected),
            }
        }

        match (types.get(want), types.get(give)) {
            (Number, Number) => Ok(()),

            (Bytes(fw), Bytes(fg)) => handle_finiteness(keep_inf, *fw, *fg, types),

            (List(fw, l_ty), List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                handle_finiteness(keep_inf, *fw, *fg, types)?;
                Type::concretize(l_ty, r_ty, types, keep_inf)
            }

            (&Func(l_par, l_ret), &Func(r_par, r_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str+ -> Str+)
                // (a -> b) <- (c -> c)
                Type::concretize(l_ret, r_ret, types, keep_inf)?;
                Type::concretize(r_par, l_par, types, keep_inf)?;
                // needs to propagate back again any change by 2nd concretize
                Type::concretize(l_ret, r_ret, types, keep_inf)
            }

            (&Pair(l_fst, l_snd), &Pair(r_fst, r_snd)) => {
                Type::concretize(l_fst, r_fst, types, keep_inf)?;
                Type::concretize(l_snd, r_snd, types, keep_inf)
            }

            (Named(w), Named(g)) => {
                // when we have ptr equality, we know we are talking twice about the exact same
                // named, in which case nothing needs to be done (it would only introduce
                // a redundant named and trs that cannot be easily removed)
                if !std::ptr::eq(w, g) {
                    // XXX: 'a=b' pseudo syntax is somewhat temporary
                    // mostly because idk which one you'd more likely want to keep
                    let u = types.named(if w == g {
                        w.clone()
                    } else {
                        format!("{w}={g}")
                    });
                    types.set(want, Transparent(u));
                    types.set(give, Transparent(u));
                }
                Ok(())
            }
            (other, Named(_)) => {
                types.set(give, other.clone());
                Ok(())
            }
            (Named(_), other) => {
                types.set(want, other.clone());
                Ok(())
            }

            _ => Err(ErrorKind::ExpectedButGot {
                expected: types.frozen(want),
                actual: types.frozen(give),
            }),
        }
    }

    /// Note that there is a class of convoluted cases for which this
    /// will return false positives; when concretization is _required_
    /// (reasonably) to determined compatibility.
    pub(crate) fn compatible(want: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match (types.get(want), types.get(give)) {
            (Number, Number) => true,

            (Bytes(fw), Bytes(fg)) => match (types.freeze_finite(*fw), types.freeze_finite(*fg)) {
                (true, false) => false,
                (_, _) => true,
            },

            (List(fw, l_ty), List(fg, r_ty)) => {
                match (types.freeze_finite(*fw), types.freeze_finite(*fg)) {
                    (true, false) => false,
                    (_, _) => Type::compatible(*l_ty, *r_ty, types),
                }
            }

            (Func(l_par, l_ret), Func(r_par, r_ret)) => {
                // parameter compare contravariantly: (Str -> c) <- (Str+ -> Str+)
                Type::compatible(*r_par, *l_par, types) && Type::compatible(*l_ret, *r_ret, types)
            }

            (Pair(l_fst, l_snd), Pair(r_fst, r_snd)) => {
                Type::compatible(*l_fst, *r_fst, types) && Type::compatible(*l_snd, *r_snd, types)
            }

            (_, Named(_)) => true,
            (Named(_), _) => true,

            _ => false,
        }
    }
}
// }}}

impl Display for FrozenType {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            FrozenType::Number => write!(f, "Num"),

            FrozenType::Bytes(true) => write!(f, "Str"),
            FrozenType::Bytes(false) => write!(f, "Str+"),

            FrozenType::List(true, has) => write!(f, "[{has}]"),
            FrozenType::List(false, has) => write!(f, "[{has}]+"),

            FrozenType::Func(par, ret) => {
                let funcarg = matches!(**par, FrozenType::Func(_, _));
                if funcarg {
                    write!(f, "(")?;
                }
                write!(f, "{par}")?;
                if funcarg {
                    write!(f, ")")?;
                }
                write!(f, " -> ")?;
                write!(f, "{ret}")
            }

            FrozenType::Pair(fst, snd) => write!(f, "({fst}, {snd})"),

            FrozenType::Named(name) => write!(f, "{name}"),
        }
    }
}
