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
pub enum Type {
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
pub struct TypeList(Vec<Type>);

#[cfg(feature = "types-snapshots")]
mod types_snapshots {
    use std::fs::File;
    use std::io::Write;
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::{LazyLock, Mutex};

    use super::*;

    static DOTS: LazyLock<Mutex<File>> = LazyLock::new(|| {
        const HTML: &str = r#"<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <script src="https://d3js.org/d3.v5.min.js"></script>
        <script src="https://cdnjs.cloudflare.com/ajax/libs/d3-graphviz/5.6.0/d3-graphviz.min.js" integrity="sha512-Le8HpIpS2Tc7SDHLM6AOgAKq6ZR4uDwLhjPSR20DtXE5dFb9xECHRwgpc1nxxnU0Dv+j6FNMoSddky5gyvI3lQ==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
        <style> body { padding: 0; margin: 0; overflow: hidden; background: #333; color: #eee; } #currentSection { font-weight: bolder; } </style>
    </head>
    <body>
        <div id="control" style="overflow: auto;">
            <h2 style="text-align: center;">(use hjkl) <span id="shownIndex"></span><span id="showSection"></span></h2>
            <ul id="listSections"></ul>
        </div>
        <div id="graph" style="text-align: center;"></div>
        <script>/*-*/;onload=function(){
            var useWidth = innerWidth, useHeight = innerHeight;
            if (innerWidth < innerHeight) {
                useHeight*= .6;
                control.style.width = innerWidth+'px';
                control.style.height = (innerHeight-useHeight-20)+'px';
            } else {
                useWidth*= .7;
                control.style.width = (innerWidth-useWidth-20)+'px';
                control.style.height = innerHeight+'px';
                control.style.float = 'left';
                graph.style.float = 'right';
            }
            graph.style.height = useHeight+'px';
            graph.style.width = useWidth+'px';
            var dots = dotLines.textContent.trim().split('\n').map(l => '#' === l[0] ? ['#', l.slice(2), 0] : 'digraph{edge[color="gray93"]node[color="gray93"fontcolor="gray93"]fontcolor="gray93"bgcolor="gray20"'+l+'}');
            for (var k = 0; k < dots.length; ++k) if ('#' === dots[k][0]) {
                dots[k][2] = listSections.childNodes.length;
                var li = listSections.appendChild(document.createElement('li'));
                li.textContent = dots[k][1];
                if (k+1 === dots.length || '#' === dots[k+1][0]) li.style.fontStyle = 'italic';
            }
            listSections.firstChild.id = 'currentSection';
            var dotIndex = 0;
            var graphviz = d3.select('#graph').graphviz().zoom(false).width(useWidth).height(useHeight).fit(true).transition(() => d3.transition('main').ease(d3.easeLinear).duration(125)).on('initEnd', show);
            function show() {
                if (dotIndex < 1) flash(), dotIndex = 1; else if (dots.length-1 < dotIndex) flash(), dotIndex = dots.length-1;
                for (var k = dotIndex; 0 < k;) if ('#' === dots[--k][0]) {
                    showSection.textContent = '/'+(dots.length-1)+' '+dots[k][1];
                    currentSection.removeAttribute('id');
                    listSections.childNodes[dots[k][2]].id = 'currentSection';
                    break;
                }
                graphviz.renderDot(dots[shownIndex.textContent = dotIndex]);
            }
            function next() { while (dots[++dotIndex] && '#' === dots[dotIndex][0]); show(); }
            function prev() { while (dots[--dotIndex] && '#' === dots[dotIndex][0]); show(); }
            function jumpnext() { while (dots[++dotIndex] && '#' !== dots[dotIndex][0]); while (dots[++dotIndex] && '#' === dots[dotIndex][0]); next(); }
            function jumpprev() { while (dots[--dotIndex] && '#' !== dots[dotIndex][0]); prev(); }
            function flash() { document.body.style.color = '#333'; setTimeout(() => document.body.style.color = '', 125); }
            var num = 0;
            onkeypress = (ev) => {
                if (!isNaN(parseInt(ev.key))) { num = num*10 + parseInt(ev.key); shownIndex.textContent = dotIndex+' ('+num+')'; return; }
                var f = { h: prev, j: jumpnext, k: jumpprev, l: next, g() { dotIndex = Math.min(num, dots.length-1); show(); } }[ev.key];
                if (f) f(); num = 0;
            }
        };</script>
        <span id="dotLines" style="display: none;">"#;
        let mut f = File::create("types-snapshots.html").unwrap();
        writeln!(f, "{HTML}").unwrap();
        Mutex::new(f)
    });
    static DEPTH: AtomicU32 = AtomicU32::new(0);

    pub(super) fn group_dot_lines(hi: String) {
        writeln!(DOTS.lock().unwrap(), "# {hi}").unwrap();
    }

    pub(super) fn update_dot(types: &TypeList, title: String) {
        let mut f = DOTS.lock().unwrap();
        write!(f, "label=\"{title}\"").unwrap();
        types
            .0
            .iter()
            .enumerate()
            .try_for_each(|(k, slot)| {
                use Type::*;
                for at in match *slot {
                    Number => vec![],
                    Bytes(fin) => vec![fin],
                    List(fin, has) => vec![fin, has],
                    Func(par, ret) => vec![par, ret],
                    Pair(fst, snd) => vec![fst, snd],
                    Named(_) => vec![],
                    Finite(_) => vec![],
                    FiniteBoth(lhs, rhs) => vec![lhs, rhs],
                    FiniteEither(lhs, rhs) => vec![lhs, rhs],
                    Transparent(u) => vec![u],
                } {
                    write!(
                        f,
                        " \"{k} :: {}\" -> \"{at} :: {}\" ",
                        string_any_type(types, k),
                        string_any_type(types, at)
                    )?;
                }
                write!(f, " \"{k} :: {}\" ", string_any_type(types, k))
            })
            .unwrap();
        writeln!(f).unwrap();
    }

    pub(super) fn string_any_type(types: &TypeList, at: TypeRef) -> String {
        DEPTH.store(0, Ordering::Relaxed);
        string_any_type_impl(types, at)
    }

    fn string_any_type_impl(types: &TypeList, at: TypeRef) -> String {
        use Type::*;
        if 12 < DEPTH.fetch_add(1, Ordering::Relaxed) {
            return "...".into();
        }
        match &types.0[at] {
            Number => "Num".into(),
            &Bytes(fin) => if types.freeze_finite(fin) {
                "Str"
            } else {
                "Str+"
            }
            .into(),
            &List(fin, has) => {
                let fin = if types.freeze_finite(fin) { "" } else { "+" };
                format!("[{}]{fin}", string_any_type_impl(types, has))
            }
            &Func(par, ret) => {
                let (op, cl) = if matches!(types.get(par), Func(_, _)) {
                    ("(", ")")
                } else {
                    ("", "")
                };
                format!(
                    "{op}{}{cl} -> {}",
                    string_any_type_impl(types, par),
                    string_any_type_impl(types, ret)
                )
            }
            &Pair(fst, snd) => format!(
                "({}, {})",
                string_any_type_impl(types, fst),
                string_any_type_impl(types, snd)
            ),
            Named(name) => name.clone(),
            &Finite(is) => {
                if is {
                    format!("{at}")
                } else {
                    "+".into()
                }
            }
            &FiniteBoth(l, r) => format!(
                "<{}&{}>",
                string_any_type_impl(types, l),
                string_any_type_impl(types, r)
            ),
            &FiniteEither(l, r) => format!(
                "<{}|{}>",
                string_any_type_impl(types, l),
                string_any_type_impl(types, r)
            ),
            &Transparent(u) => format!("({})", string_any_type_impl(types, u)),
        }
    }
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
