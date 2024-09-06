use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};

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
}

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
        match types.get(at) {
            Number => "Num".into(),
            Bytes(fin) => if types.freeze_finite(*fin) {
                "Str"
            } else {
                "Str+"
            }
            .into(),
            List(fin, has) => if types.freeze_finite(*fin) {
                format!("[{}]", string_any_type_impl(types, *has))
            } else {
                format!("[{}]+", string_any_type_impl(types, *has))
            }
            .into(),
            Func(par, ret) => {
                let funcarg = matches!(types.get(*par), Func(_, _));
                if funcarg {
                    format!(
                        "({}) -> {}",
                        string_any_type_impl(types, *par),
                        string_any_type_impl(types, *ret)
                    )
                } else {
                    format!(
                        "{} -> {}",
                        string_any_type_impl(types, *par),
                        string_any_type_impl(types, *ret)
                    )
                }
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
    pub fn transaction_group(&self, hi: String) {
        #[cfg(feature = "types-snapshots")]
        types_snapshots::group_dot_lines(hi);
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
        self.slots[at].as_ref().unwrap()
    }

    pub(crate) fn set(&mut self, at: TypeRef, ty: Type) {
        self.slots[at] = Some(ty);
        #[cfg(feature = "types-snapshots")]
        types_snapshots::update_dot(
            &self,
            format!("set {at} :: {}", types_snapshots::string_any_type(self, at)),
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
            self.free_slots = 0;
        }
    }

    pub fn number(&self) -> TypeRef {
        NUMBER_TYPEREF
    }
    pub fn bytes(&mut self, finite: Boundedness) -> TypeRef {
        if FINITE_TYPEREF == finite {
            STRFIN_TYPEREF
        } else {
            self.push(Type::Bytes(finite))
        }
    }
    pub fn list(&mut self, finite: Boundedness, item: TypeRef) -> TypeRef {
        self.push(Type::List(finite, item))
    }
    pub fn func(&mut self, par: TypeRef, ret: TypeRef) -> TypeRef {
        self.push(Type::Func(par, ret))
    }
    pub fn pair(&mut self, fst: TypeRef, snd: TypeRef) -> TypeRef {
        self.push(Type::Pair(fst, snd))
    }
    pub fn named(&mut self, name: String) -> TypeRef {
        self.push(Type::Named(name.to_string()))
    }
    pub fn finite(&mut self, finite: bool) -> Boundedness {
        if finite {
            FINITE_TYPEREF
        } else {
            self.push(Type::Finite(false))
        }
    }
    pub fn either(&mut self, left: Boundedness, right: Boundedness) -> Boundedness {
        self.push(Type::FiniteEither(left, right))
    }
    pub fn both(&mut self, left: Boundedness, right: Boundedness) -> Boundedness {
        self.push(Type::FiniteBoth(left, right))
    }

    pub(crate) fn duplicate(
        &mut self,
        at: TypeRef,
        already_done: &mut HashMap<usize, usize>,
    ) -> TypeRef {
        if let Some(done) = already_done.get(&at) {
            return *done;
        }
        let r = match *self.get(at) {
            Type::Number => self.number(),
            Type::Bytes(f) => {
                let f = self.duplicate(f, already_done);
                self.bytes(f)
            }
            Type::List(f, i) => {
                let (f, i) = (
                    self.duplicate(f, already_done),
                    self.duplicate(i, already_done),
                );
                self.list(f, i)
            }
            Type::Func(p, r) => {
                let (p, r) = (
                    self.duplicate(p, already_done),
                    self.duplicate(r, already_done),
                );
                self.func(p, r)
            }
            Type::Pair(f, s) => {
                let (f, s) = (
                    self.duplicate(f, already_done),
                    self.duplicate(s, already_done),
                );
                self.pair(f, s)
            }
            Type::Named(ref n) => {
                let n = n.clone();
                self.named(n)
            }
            Type::Finite(i) => self.finite(i),
            Type::FiniteBoth(l, r) => {
                let (l, r) = (
                    self.duplicate(l, already_done),
                    self.duplicate(r, already_done),
                );
                self.both(l, r)
            }
            Type::FiniteEither(l, r) => {
                let (l, r) = (
                    self.duplicate(l, already_done),
                    self.duplicate(r, already_done),
                );
                self.either(l, r)
            }
        };
        already_done.insert(at, r);
        r
    }

    fn freeze_finite(&self, at: Boundedness) -> bool {
        match *self.get(at) {
            Type::Finite(b) => b,
            Type::FiniteBoth(l, r) => self.freeze_finite(l) && self.freeze_finite(r),
            Type::FiniteEither(l, r) => self.freeze_finite(l) || self.freeze_finite(r),

            Type::Number
            | Type::Bytes(_)
            | Type::List(_, _)
            | Type::Func(_, _)
            | Type::Pair(_, _)
            | Type::Named(_) => unreachable!(),
        }
    }

    pub(crate) fn frozen(&self, at: TypeRef) -> FrozenType {
        match self.get(at) {
            Type::Number => FrozenType::Number,
            Type::Bytes(b) => FrozenType::Bytes(self.freeze_finite(*b)),
            Type::List(b, items) => {
                FrozenType::List(self.freeze_finite(*b), Box::new(self.frozen(*items)))
            }
            Type::Func(par, ret) => {
                FrozenType::Func(Box::new(self.frozen(*par)), Box::new(self.frozen(*ret)))
            }
            Type::Pair(fst, snd) => {
                FrozenType::Pair(Box::new(self.frozen(*fst)), Box::new(self.frozen(*snd)))
            }
            Type::Named(name) => FrozenType::Named(name.clone()),
            Type::Finite(_) | Type::FiniteBoth(_, _) | Type::FiniteEither(_, _) => unreachable!(),
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
            &Type::Func(want, ret) => Type::concretize(want, give, types, false).map(|()| ret),
            _ => Err(ErrorKind::NotFunc {
                actual_type: types.frozen(func),
            }),
        }
    }

    /// Same idea as `applied` but doesn't mutate anything and doesn't fail but returns true/false.
    /// (! see comment on underlying `compatible` itself)
    pub(crate) fn applicable(func: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match types.get(func) {
            &Type::Func(want, _) => Type::compatible(want, give, types),
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
        use Type::*;

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
                        types.set(fw, Finite(true));
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
    fn compatible(want: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        use Type::*;
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
