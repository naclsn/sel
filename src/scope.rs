use std::collections::HashMap;

use crate::error::Location;
use crate::parse::Tree;
use crate::types::{TypeList, TypeRef};

#[derive(Debug)]
pub enum Bidoof {
    Builtin(fn(&mut TypeList) -> TypeRef, &'static str),
    Defined(Tree, String),
    Binding(Location, TypeRef),
}

#[derive(Debug)]
pub struct Scope {
    pub(crate) parent: *const Scope,
    names: HashMap<String, Bidoof>,
}

impl Bidoof {
    pub fn make_type(&self, types: &mut TypeList) -> TypeRef {
        match self {
            Bidoof::Builtin(mkty, _) => mkty(types),
            Bidoof::Defined(val, _) => types.duplicate(val.ty, &mut HashMap::new()),
            Bidoof::Binding(_, ty) => types.duplicate(*ty, &mut HashMap::new()),
        }
    }

    pub fn get_loc(&self) -> Option<&Location> {
        match self {
            Bidoof::Builtin(_, _) => None,
            Bidoof::Defined(val, _) => Some(&val.loc),
            Bidoof::Binding(loc, _) => Some(loc),
        }
    }

    pub fn get_desc(&self) -> Option<&str> {
        match self {
            Bidoof::Builtin(_, desc) => Some(desc),
            Bidoof::Defined(_, desc) => Some(desc),
            Bidoof::Binding(_, _) => None,
        }
    }
}

impl Scope {
    pub(crate) fn new(parent: Option<&Scope>) -> Scope {
        Scope {
            parent: parent
                .map(|p| p as *const Scope)
                .unwrap_or(std::ptr::null()),
            names: HashMap::new(),
        }
    }

    /// parent-most englobing scope
    pub fn global(&self) -> &Scope {
        unsafe { self.parent.as_ref() }.unwrap_or(self)
    }

    /// does not insert if the name already existed, returns a reference to the previous value
    pub(crate) fn declare(&mut self, name: String, value: Bidoof) -> Option<&Bidoof> {
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

    pub fn lookup(&self, name: &str) -> Option<&Bidoof> {
        self.names
            .get(name)
            .or_else(|| unsafe { self.parent.as_ref() }?.lookup(name))
    }

    pub fn iter(&self) -> <&HashMap<String, Bidoof> as IntoIterator>::IntoIter {
        self.names.iter()
    }

    pub fn iter_rec(&self) -> ScopeRecIter {
        ScopeRecIter {
            entries: self.names.iter(),
            parent: unsafe { self.parent.as_ref() },
        }
    }
}

pub struct ScopeRecIter<'a> {
    entries: <&'a HashMap<String, Bidoof> as IntoIterator>::IntoIter,
    parent: Option<&'a Scope>,
}

impl<'a> Iterator for ScopeRecIter<'a> {
    type Item = (&'a String, &'a Bidoof);

    fn next(&mut self) -> Option<Self::Item> {
        self.entries.next().or_else(|| {
            *self = self.parent?.iter_rec();
            self.next()
        })
    }
}

impl IntoIterator for Scope {
    type IntoIter = <HashMap<String, Bidoof> as IntoIterator>::IntoIter;
    type Item = <Self::IntoIter as Iterator>::Item;

    fn into_iter(self) -> Self::IntoIter {
        self.names.into_iter()
    }
}

impl<'a> IntoIterator for &'a Scope {
    type IntoIter = <&'a HashMap<String, Bidoof> as IntoIterator>::IntoIter;
    type Item = <Self::IntoIter as Iterator>::Item;

    fn into_iter(self) -> Self::IntoIter {
        self.names.iter()
    }
}
