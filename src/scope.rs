use std::collections::HashMap;
use std::fs::File;
use std::io::{Read, Result as IoResult};
use std::marker::PhantomPinned;
use std::mem;
use std::ops::Range;
use std::path::{Path, PathBuf};
use std::pin::Pin;
use std::ptr;

use crate::builtin;
use crate::parse::Tree;
use crate::types::{TypeList, TypeRef};

pub struct Global {
    pub registry: SourceRegistry,
    pub types: TypeList,
    pub scope: Scope,
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

// source registry {{{
pub type SourceRef = usize;

#[derive(PartialEq, Debug, Clone)]
pub struct Location(pub SourceRef, pub Range<usize>);

pub struct Source {
    pub path: PathBuf,
    pub bytes: Vec<u8>,
    line_map: Vec<Range<usize>>,
}

#[derive(Default)]
pub struct SourceRegistry(Vec<Source>);

impl SourceRegistry {
    /// Note: the given path is taken as is (ie not canonicalize) and not checked for duplication
    pub fn add_bytes(&mut self, name: impl AsRef<Path>, bytes: Vec<u8>) -> SourceRef {
        let line_map = Source::compute_line_map(&bytes);
        self.0.push(Source {
            path: name.as_ref().to_owned(),
            bytes,
            line_map,
        });
        self.0.len() - 1
    }

    pub fn add(&mut self, name: impl AsRef<Path>) -> IoResult<SourceRef> {
        let name = name.as_ref().canonicalize()?;
        if let Some(found) = self.0.iter().position(|c| c.path == name) {
            return Ok(found);
        }
        let mut bytes = Vec::new();
        File::open(&name)?.read_to_end(&mut bytes)?;
        Ok(self.add_bytes(name, bytes))
    }

    pub fn get(&self, at: SourceRef) -> &Source {
        &self.0[at]
    }
}

impl Source {
    fn compute_line_map(bytes: &[u8]) -> Vec<Range<usize>> {
        let mut r = Vec::new();

        let mut it = bytes.iter();
        let mut at = 0;
        while let Some(nl) = it.position(|c| b'\n' == *c) {
            r.push(at..at + nl);
            at += nl + 1;
        }
        if at < bytes.len() {
            r.push(at..bytes.len());
        }

        r
    }

    pub fn get_containing_lnum(&self, k: usize) -> Option<usize> {
        self.line_map
            .iter()
            .position(|r| r.contains(&k))
            .map(|l| l + 1)
    }

    /// the tuple's first item is the same as if `source.get_containing_lnum(k.start)`
    pub fn get_containing_lines(&self, k: &Range<usize>) -> Option<(usize, &[Range<usize>])> {
        let mut it = self.line_map.iter();
        let start = it.position(|r| k.start <= r.end)?;
        let end = start + it.position(|r| k.end < r.start).unwrap_or(0);
        Some((start + 1, &self.line_map[start..=end]))
    }
}
// }}}

impl Global {
    pub fn with_builtin() -> Global {
        Global {
            registry: SourceRegistry::default(),
            types: TypeList::default(),
            scope: builtin::scope(),
        }
    }
}

// scope {{{
impl ScopeItem {
    pub fn make_type(&self, types: &mut TypeList) -> TypeRef {
        match self {
            ScopeItem::Builtin(mkty, _) => mkty(types),
            ScopeItem::Defined(val, _) => types.duplicate(val.ty, &mut HashMap::new()),
            ScopeItem::Binding(_, ty) => *ty, //types.duplicate(*ty, &mut HashMap::new()),
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
pub(crate) struct MustRestore(Scope, PhantomPinned);

impl Default for Scope {
    fn default() -> Scope {
        Scope {
            parent: ptr::null(),
            names: HashMap::default(),
        }
    }
}

impl Scope {
    pub(crate) fn new(parent: &Scope) -> Scope {
        Scope {
            parent: parent as *const Scope,
            names: HashMap::new(),
        }
    }

    /// returns the parent scope (previously `self`)
    /// which is expected to be restored with `restore_from_parent`
    pub(crate) fn make_into_child(&mut self) -> Pin<Box<MustRestore>> {
        let parent = Box::pin(MustRestore(mem::take(self), PhantomPinned));
        self.parent = &parent.0 as *const Scope;
        parent
    }
    /// see `make_into_child`
    pub(crate) fn restore_from_parent(&mut self, parent: Pin<Box<MustRestore>>) {
        // im not sure what im doin
        mem::swap(self, &mut unsafe { Pin::into_inner_unchecked(parent) }.0);
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
// }}}
