use std::collections::HashMap;

use crate::builtin;
use crate::error::{Location, SourceRegistry};
use crate::parse::Tree;
use crate::types::{TypeList, TypeRef};

pub struct Global {
    pub registry: SourceRegistry,
    pub types: TypeList,
    pub scope: Scope,
}

impl Global {
    pub fn new() -> Global {
        Global {
            registry: SourceRegistry::new(),
            types: TypeList::default(),
            scope: builtin::scope(),
        }
    }
}

#[derive(Debug)]
pub enum ScopeItem {
    Builtin(fn(&mut TypeList) -> TypeRef, &'static str),
    Defined(Tree, String),
    Binding(Location, TypeRef),
}

#[derive(Debug)]
pub struct Scope {
    parent: *const Scope,
    names: HashMap<String, ScopeItem>,
}

impl ScopeItem {
    pub fn make_type(&self, types: &mut TypeList) -> TypeRef {
        match self {
            ScopeItem::Builtin(mkty, _) => mkty(types),
            ScopeItem::Defined(val, _) => types.duplicate(val.ty, &mut HashMap::new()),
            ScopeItem::Binding(_, ty) => types.duplicate(*ty, &mut HashMap::new()),
        }
    }

    pub fn get_loc(&self) -> Option<&Location> {
        match self {
            ScopeItem::Builtin(_, _) => None,
            ScopeItem::Defined(val, _) => Some(&val.loc),
            ScopeItem::Binding(loc, _) => Some(loc),
        }
    }

    pub fn get_desc(&self) -> Option<&str> {
        match self {
            ScopeItem::Builtin(_, desc) => Some(desc),
            ScopeItem::Defined(_, desc) => Some(desc),
            ScopeItem::Binding(_, _) => None,
        }
    }
}

// YYY: weird pattern, surely there is better :/
pub(crate) struct MustRestore(Scope);

impl Scope {
    pub(crate) fn new(parent: Option<&Scope>) -> Scope {
        Scope {
            parent: parent
                .map(|p| p as *const Scope)
                .unwrap_or(std::ptr::null()),
            names: HashMap::new(),
        }
    }

    /// returns the parent scope (previously `self`)
    /// which is expected to be restored with `restore_from_parent`
    pub(crate) fn make_into_child(&mut self) -> MustRestore {
        let parent = std::mem::replace(self, Scope::new(None));
        self.parent = &parent as *const Scope;
        MustRestore(parent)
    }
    /// see `make_into_child`
    pub(crate) fn restore_from_parent(&mut self, parent: MustRestore) {
        *self = parent.0;
    }

    /// parent-most englobing scope
    pub fn global(&self) -> &Scope {
        unsafe { self.parent.as_ref() }.unwrap_or(self)
    }

    /// does not insert if the name already existed, returns a reference to the previous value
    pub(crate) fn declare(&mut self, name: String, value: ScopeItem) -> Option<&ScopeItem> {
        let mut already = false;
        let r = self
            .names
            .entry(name)
            .and_modify(|_| already = true)
            .or_insert_with(|| value);
        if already {
            Some(r)
        } else {
            None
        }
    }

    pub fn lookup(&self, name: &str) -> Option<&ScopeItem> {
        self.names
            .get(name)
            .or_else(|| unsafe { self.parent.as_ref() }?.lookup(name))
    }

    pub fn iter(&self) -> <&HashMap<String, ScopeItem> as IntoIterator>::IntoIter {
        self.names.iter()
    }

    pub fn iter_rec(&self) -> ScopeRecIter {
        ScopeRecIter {
            entries: self.names.iter(),
            parent: unsafe { self.parent.as_ref() },
        }
    }
}

// iters {{{
pub struct ScopeRecIter<'a> {
    entries: <&'a HashMap<String, ScopeItem> as IntoIterator>::IntoIter,
    parent: Option<&'a Scope>,
}

impl<'a> Iterator for ScopeRecIter<'a> {
    type Item = (&'a String, &'a ScopeItem);

    fn next(&mut self) -> Option<Self::Item> {
        self.entries.next().or_else(|| {
            *self = self.parent?.iter_rec();
            self.next()
        })
    }
}

impl IntoIterator for Scope {
    type IntoIter = <HashMap<String, ScopeItem> as IntoIterator>::IntoIter;
    type Item = <Self::IntoIter as Iterator>::Item;

    fn into_iter(self) -> Self::IntoIter {
        self.names.into_iter()
    }
}

impl<'a> IntoIterator for &'a Scope {
    type IntoIter = <&'a HashMap<String, ScopeItem> as IntoIterator>::IntoIter;
    type Item = <Self::IntoIter as Iterator>::Item;

    fn into_iter(self) -> Self::IntoIter {
        self.names.iter()
    }
}
// }}}
