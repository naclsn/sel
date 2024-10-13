use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Peekable};

use crate::error::ErrorKind;

const NUMBER_TYPEREF: usize = 0;
const STRFIN_TYPEREF: usize = 1;
const FINITE_TYPEREF: usize = 2;

pub type TypeRef = usize;
pub type Boundedness = usize;

#[derive(Debug, Clone)]
pub(crate) enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, TypeRef),
    Func(TypeRef, TypeRef),
    Pair(TypeRef, TypeRef),
    Named(String),
    Finite(bool),
    FiniteBoth(Boundedness, Boundedness),
    FiniteEither(Boundedness, Boundedness),
    Transparent(TypeRef),
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

#[derive(Clone)]
pub struct TypeList {
    slots: Vec<Option<Type>>,
    free_slots: usize,
}

#[cfg(feature = "types-snapshots")]
mod types_snapshots {
    use std::fs::File;
    use std::io::Write;
    use std::sync::OnceLock;

    use super::*;

    static mut DOTS: OnceLock<File> = OnceLock::new();
    static mut LIMIT: usize = 0;

    pub(super) fn group_dot_lines(hi: String) {
        writeln!(unsafe {
        DOTS.get_or_init(|| {
            let mut f = File::create("types-snapshots.html").unwrap();
            writeln!(f, "{}", r#"<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <script src="https://d3js.org/d3.v5.min.js"></script>
        <script src="https://unpkg.com/@hpcc-js/wasm@0.3.11/dist/index.min.js"></script>
        <script src="https://unpkg.com/d3-graphviz@3.0.5/build/d3-graphviz.js"></script>
        <style> body { padding: 0; margin: 0; overflow: hidden; background: #333; color: #eee; } </style>
    </head>
    <body>
        <h2 style="position: absolute; top: 3rem; left: 50%; transform: translateX(-50%);">
            (use hjkl)
            <span id="shownIndex"></span>
            <span id="showSection"></span>
        </h2>
        <div id="graph" style="text-align: center;"></div>
        <script>/*-*/;onload=function(){
            var dots = dotLines.textContent.trim().split('\n').map(l => '#' === l[0] ? l : 'digraph{edge[color="gray93"]node[color="gray93"fontcolor="gray93"]fontcolor="gray93"bgcolor="gray20"'+l+'}');
            var dotIndex = 0;
            var graphviz = d3.select('#graph').graphviz().zoom(false).width(innerWidth).height(innerHeight).fit(true).transition(() => d3.transition('main').ease(d3.easeLinear).duration(125)).on('initEnd', show);
            function show() {
                if (dotIndex < 1) flash(), dotIndex = 1; else if (dots.length-1 < dotIndex) flash(), dotIndex = dots.length-1;
                for (var k = dotIndex; 0 < k;) if ('#' === dots[--k][0]) { showSection.textContent = '/'+(dots.length-1)+' '+dots[k]; break; }
                graphviz.renderDot(dots[shownIndex.textContent = dotIndex]);
            }
            function next() { while (dots[++dotIndex] && '#' === dots[dotIndex][0]); show(); }
            function prev() { while (dots[--dotIndex] && '#' === dots[dotIndex][0]); show(); }
            function jumpnext() { while (dots[++dotIndex] && '#' !== dots[dotIndex][0]); while (dots[++dotIndex] && '#' !== dots[dotIndex][0]); prev(); }
            function jumpprev() { while (dots[--dotIndex] && '#' !== dots[dotIndex][0]); prev(); }
            function flash() { document.body.style.color = '#333'; setTimeout(() => document.body.style.color = '', 125); }
            var num = 0;
            onkeypress = (ev) => {
                if (!isNaN(parseInt(ev.key))) { num = num*10 + parseInt(ev.key); shownIndex.textContent = dotIndex+' ('+num+')'; return; }
                var f = { h: prev, j: jumpnext, k: jumpprev, l: next, g() { dotIndex = Math.min(num, dots.length-1); show(); } }[ev.key];
                if (f) f(); num = 0;
            }
        };</script>
        <span id="dotLines" style="display: none;">"#)
                .unwrap();
                f
            });
            DOTS.get_mut().unwrap()
        }, "# {hi}").unwrap();
    }

    pub(super) fn update_dot(types: &TypeList, title: String) {
        let f = unsafe { DOTS.get_mut().unwrap() };
        write!(f, "label=\"{title}\"").unwrap();
        types
            .slots
            .iter()
            .enumerate()
            .try_for_each(|(k, slot)| {
                if let Some(slot) = slot {
                    use Type::*;
                    for at in match slot {
                        &Number => vec![],
                        &Bytes(fin) => vec![fin],
                        &List(fin, has) => vec![fin, has],
                        &Func(par, ret) => vec![par, ret],
                        &Pair(fst, snd) => vec![fst, snd],
                        &Named(_) => vec![],
                        &Finite(_) => vec![],
                        &FiniteBoth(lhs, rhs) => vec![lhs, rhs],
                        &FiniteEither(lhs, rhs) => vec![lhs, rhs],
                        &Transparent(u) => vec![u],
                    } {
                        write!(
                            f,
                            " \"{k} :: {}\" -> \"{at} :: {}\" ",
                            string_any_type(types, k),
                            string_any_type(types, at)
                        )?;
                    }
                    write!(f, " \"{k} :: {}\" ", string_any_type(types, k))
                } else {
                    write!(f, " \"{k} :: []\"*/")
                }
            })
            .unwrap();
        writeln!(f).unwrap();
    }

    pub(super) fn string_any_type(types: &TypeList, at: TypeRef) -> String {
        unsafe { LIMIT = 0 };
        string_any_type_impl(types, at)
    }

    fn string_any_type_impl(types: &TypeList, at: TypeRef) -> String {
        use Type::*;
        if unsafe {
            LIMIT += 1;
            12 < LIMIT
        } {
            return "...".into();
        }
        match types.slots[at].as_ref().unwrap() {
            Number => "Num".into(),
            Bytes(fin) => if types.freeze_finite(*fin) {
                "Str"
            } else {
                "Str+"
            }
            .into(),
            List(fin, has) => {
                let fin = if types.freeze_finite(*fin) { "" } else { "+" };
                format!("[{}]{fin}", string_any_type_impl(types, *has))
            }
            Func(par, ret) => {
                let (op, cl) = if matches!(types.get(*par), Func(_, _)) {
                    ("(", ")")
                } else {
                    ("", "")
                };
                format!(
                    "{op}{}{cl} -> {}",
                    string_any_type_impl(types, *par),
                    string_any_type_impl(types, *ret)
                )
            }
            Pair(fst, snd) => format!(
                "({}, {})",
                string_any_type_impl(types, *fst),
                string_any_type_impl(types, *snd)
            ),
            Named(name) => format!("{name}"),
            Finite(is) => {
                if *is {
                    format!("{at}")
                } else {
                    "+".into()
                }
            }
            FiniteBoth(l, r) => format!(
                "<{}&{}>",
                string_any_type_impl(types, *l),
                string_any_type_impl(types, *r)
            ),
            FiniteEither(l, r) => format!(
                "<{}|{}>",
                string_any_type_impl(types, *l),
                string_any_type_impl(types, *r)
            ),
            Transparent(u) => format!("({})", string_any_type_impl(types, *u)),
        }
    }
}

impl Default for TypeList {
    fn default() -> Self {
        let r = TypeList {
            slots: vec![
                Some(Type::Number),       // [NUMBER_TYPEREF]
                Some(Type::Bytes(2)),     // [STRFIN_TYPEREF]
                Some(Type::Finite(true)), // [FINITE_TYPEREF]
            ],
            free_slots: 0,
        };
        r.transaction_group("default".into());
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(&r, "default".into());
        r
    }
}

// types vec with holes, also factory and such {{{
impl TypeList {
    pub fn transaction_group(&self, _hi: String) {
        #[cfg(feature = "types-snapshots")]
        types_snapshots::group_dot_lines(_hi);
    }

    fn push(&mut self, it: Type) -> TypeRef {
        let k = if let Some((k, o)) = match self.free_slots {
            0 => None,
            _ => self.slots.iter_mut().enumerate().find(|p| p.1.is_none()),
        } {
            *o = Some(it);
            self.free_slots -= 1;
            k
        } else {
            self.slots.push(Some(it));
            self.slots.len() - 1
        };
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            &self,
            format!("push {k} :: {}", types_snapshots::string_any_type(self, k)),
        );
        k
    }

    pub(crate) fn get(&self, at: TypeRef) -> &Type {
        match self.slots[at].as_ref().unwrap() {
            Transparent(u) => self.get(*u),
            r => r,
        }
    }

    pub(crate) fn set(&mut self, at: TypeRef, ty: Type) {
        let ty = match ty {
            Number => Transparent(NUMBER_TYPEREF),
            Bytes(FINITE_TYPEREF) => Transparent(STRFIN_TYPEREF),
            Finite(true) => Transparent(FINITE_TYPEREF),
            Transparent(u) => {
                if at == u {
                    return;
                }
                // if setting [at] to a transparent _to a transparent_, remove one indirection level
                match self.slots[u] {
                    Some(Transparent(w)) => Transparent(w),
                    _ => ty,
                }
            }
            _ => ty,
        };
        if let Some(Transparent(u)) = self.slots[at] {
            return self.set(u, ty);
        }
        #[cfg(feature = "types-snapshots")]
        let was = types_snapshots::string_any_type(self, at);
        let prev = self.slots[at].replace(ty);
        assert!(
            matches!(prev, Some(Named(_)) | None),
            "assertion failed: should only be assigning to variable types, not constants {:?}",
            prev.unwrap()
        );
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            &self,
            format!(
                "set {at} :: {} (was {})",
                types_snapshots::string_any_type(self, at),
                was
            ),
        );
    }

    pub(crate) fn plop(&mut self, at: TypeRef) {
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            &self,
            format!(
                "plop {at} :: {}",
                types_snapshots::string_any_type(self, at)
            ),
        );
        self.slots[at] = None;
        self.free_slots += 1;
        while let Some(None) = self.slots.last() {
            self.slots.pop();
            self.free_slots -= 1;
        }
    }

    pub fn number(&self) -> TypeRef {
        NUMBER_TYPEREF
    }
    pub fn bytes(&mut self, finite: Boundedness) -> TypeRef {
        if FINITE_TYPEREF == finite {
            STRFIN_TYPEREF
        } else {
            self.push(Bytes(finite))
        }
    }
    pub fn list(&mut self, finite: Boundedness, item: TypeRef) -> TypeRef {
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
    pub fn finite(&mut self, finite: bool) -> Boundedness {
        if finite {
            FINITE_TYPEREF
        } else {
            self.push(Finite(false))
        }
    }
    pub fn either(&mut self, left: Boundedness, right: Boundedness) -> Boundedness {
        self.push(FiniteEither(left, right))
    }
    pub fn both(&mut self, left: Boundedness, right: Boundedness) -> Boundedness {
        self.push(FiniteBoth(left, right))
    }

    // (ofc does not support pseudo-notations)
    pub fn parse_str(&mut self, s: &str) -> Option<TypeRef> {
        // janky implementation with technical limitation of 20 named types
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
        let r = match self.slots[at].as_ref().unwrap() {
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
            &Transparent(u) => {
                // 2 cases:
                // - the underlying type has already been duplicated => use `already_done`
                // - this is the first access => first duplicate `under`
                let u = already_done
                    .get(&u)
                    .copied()
                    .unwrap_or_else(|| self.duplicate(u, already_done));
                self.push(Transparent(u))
            }
        };

        already_done.insert(at, r);
        r
    }

    fn freeze_finite(&self, at: Boundedness) -> bool {
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

    pub(crate) fn frozen(&self, at: TypeRef) -> FrozenType {
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
            fw: Boundedness,
            fg: Boundedness,
            types: &mut TypeList,
        ) -> Result<(), ErrorKind> {
            match (types.freeze_finite(fw), types.freeze_finite(fg)) {
                (false, false) | (true, true) => Ok(()),
                (false, true) => {
                    if !keep_inf {
                        // intentionally working around `set` to skip unneeded complexity
                        // we know anyways that we are replacing a 'finite' with an other one
                        // this is maybe discussable as it can break 'finite-both/either' links..
                        types.slots[fw] = Some(Finite(true));
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
                // XXX: 'a=b' pseudo syntax is somewhat temporary
                types.set(want, Named(format!("{w}={g}")));
                types.set(give, Transparent(want));
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
