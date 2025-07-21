use insta::{assert_debug_snapshot, assert_snapshot};

use crate::error::ErrorList;
use crate::interp;
use crate::lex::{Lexer, TokenKind};
use crate::parse::{self, Tree};
use crate::scope::Global;
use crate::scope::ScopeItem;
use crate::types::FrozenType;
use crate::types::{Boundedness, TypeList, TypeRef};
use crate::{mkmkty, mkmktyty}; // from src/builtin.rs

macro_rules! with_declared {
    ($g:ident, $name:ident :: $($ty:tt)*) => {
        $g.scope.declare(
            stringify!($name).into(),
            ScopeItem::Builtin(mkmkty!($($ty)*), ""),
        );
    }
}

#[test]
fn lexing() {
    fn t(script: &[u8]) -> String {
        let mut end = true;
        Lexer::new(0, script.iter().copied())
            .take_while(|t| {
                if !end {
                    false
                } else {
                    end = TokenKind::End != t.1;
                    true
                }
            })
            .fold(String::new(), |mut acc, cur| {
                acc += &format!("{cur:?}\n");
                acc
            })
    }

    // src/snapshots/sel__tests__lexing.snap
    assert_snapshot!(t(b""));
    // src/snapshots/sel__tests__lexing-2.snap
    assert_snapshot!(t(b"coucou"));
    // src/snapshots/sel__tests__lexing-3.snap
    assert_snapshot!(t(b"[a 1 b, {w, x, y, z}, 0.5]"));
    // src/snapshots/sel__tests__lexing-4.snap
    assert_snapshot!(t(b":hay: :hey:: not hay: :: :::: fin"));
    // src/snapshots/sel__tests__lexing-5.snap
    assert_snapshot!(t(b"42 :*: 0x2a 0b101010 0o52"));
    // src/snapshots/sel__tests__lexing-6.snap
    assert_snapshot!(t(b"oh-no; don't let use be the def of cat!"));
    // src/snapshots/sel__tests__lexing-7.snap
    assert_snapshot!(t(b"
normal
# comment
normal again
#-comment not comment
#- comment again not comment
a #- [ba ba ba] b
a #- 239084 b
a #- :yeeesldkfj: b
a #- {one, [two], [{three}]} b
a #- {one, #-[two], [{three}]} b
{heyo, #-baba, owieur} # the ',' is also commented
{heyo, baba, #-owieur} # the '}' isn't
"));
}

#[test]
fn parsing() {
    fn t(script: &[u8]) -> Result<(FrozenType, Tree), ErrorList> {
        let mut global = Global::with_builtin();

        with_declared!(global, add     :: 0          ; Num -> Num -> Num                         );
        with_declared!(global, apply   :: 0, a, b    ; (a -> b) -> a -> b                        );
        with_declared!(global, const   :: 0, a, b    ; a -> b -> a                               );
        with_declared!(global, div     :: 0          ; Num -> Num -> Num                         );
        with_declared!(global, flip    :: 0, a, b, c ; (a -> b -> c) -> b -> a -> c              );
        with_declared!(global, fst     :: 0, a, b    ; (a, b) -> a                               );
        with_declared!(global, iterate :: 1, a       ; (a -> a) -> a -> [a]+1                    );
        with_declared!(global, join    :: 2          ; Str -> [Str+1]+2 -> Str+1&2               );
        with_declared!(global, ln      :: 1          ; Str+1 -> Str+1                            );
        with_declared!(global, map     :: 1, a, b    ; (a -> b) -> [a]+1 -> [b]+1                );
        with_declared!(global, repeat  :: 1, a       ; a -> [a]+1                                );
        with_declared!(global, snd     :: 0, a, b    ; (a, b) -> b                               );
        with_declared!(global, split   :: 1          ; Str -> Str+1 -> [Str+1]+1                 );
        with_declared!(global, zipwith :: 2, a, b, c ; (a -> b -> c) -> [a]+1 -> [b]+2 -> [c]+1|2);

        let source = global.registry.add_bytes("<test>", script.into());
        let result = parse::process(source, &mut global);
        result
            .tree
            .map(|t| (global.types.frozen(t.ty), t))
            .ok_or(result.errors)
    }

    // src/snapshots/sel__tests__parsing.snap
    assert_debug_snapshot!(t(b""));
    // src/snapshots/sel__tests__parsing-2.snap
    assert_debug_snapshot!(t(b"-"));
    // src/snapshots/sel__tests__parsing-3.snap
    assert_debug_snapshot!(t(b"-, split:-:, map[tonum, add1, tostr], join:+:"));
    // src/snapshots/sel__tests__parsing-4.snap
    assert_debug_snapshot!(t(b"tonum, add234121, tostr, ln"));
    // src/snapshots/sel__tests__parsing-5.snap
    assert_debug_snapshot!(t(b"[tonum, add234121, tostr] :13242:"));
    // src/snapshots/sel__tests__parsing-6.snap
    assert_debug_snapshot!(t(b"{repeat 1, {}}"));
    // src/snapshots/sel__tests__parsing-7.snap
    assert_debug_snapshot!(t(b"add 1, tostr"));
    // src/snapshots/sel__tests__parsing-8.snap
    assert_debug_snapshot!(t(b"zipwith add {1}"));
    // src/snapshots/sel__tests__parsing-9.snap
    assert_debug_snapshot!(t(b"{repeat 1, {}}"));
    // src/snapshots/sel__tests__parsing-10.snap
    assert_debug_snapshot!(t(b"{{}, repeat 1}"));
    // src/snapshots/sel__tests__parsing-11.snap
    assert_debug_snapshot!(t(b"{{42}, repeat 1}"));
    // src/snapshots/sel__tests__parsing-12.snap
    assert_debug_snapshot!(t(b"{1, 2, 3, :soleil:}, ln"));
    // src/snapshots/sel__tests__parsing-13.snap
    assert_debug_snapshot!(t(b"add 1 :2:"));
    // src/snapshots/sel__tests__parsing-14.snap
    assert_debug_snapshot!(t(b":42:, add 1"));
    // src/snapshots/sel__tests__parsing-15.snap
    assert_debug_snapshot!(t(b"add 1, tostr"));
    // src/snapshots/sel__tests__parsing-16.snap
    assert_debug_snapshot!(t(b"add 1, tonum"));
    // src/snapshots/sel__tests__parsing-17.snap
    assert_debug_snapshot!(t(b"prout 1, caca"));
    // src/snapshots/sel__tests__parsing-18.snap
    assert_debug_snapshot!(t(b"_, split_, map_, join_"));
    // src/snapshots/sel__tests__parsing-19.snap
    assert_debug_snapshot!(t(b"{const, _, add, map}"));
    // src/snapshots/sel__tests__parsing-20.snap
    assert_debug_snapshot!(t(b"2 -"));
    // src/snapshots/sel__tests__parsing-21.snap
    assert_debug_snapshot!(t(b"- 2"));
    // src/snapshots/sel__tests__parsing-22.snap
    assert_debug_snapshot!(t(b"add 1 2 3"));
    // src/snapshots/sel__tests__parsing-23.snap
    assert_debug_snapshot!(t(b"3, add 1 2"));
    // src/snapshots/sel__tests__parsing-24.snap
    assert_debug_snapshot!(t(b":1:, tostr, _"));
    // src/snapshots/sel__tests__parsing-25.snap
    assert_debug_snapshot!(t(b"iterate [_ 1, add 1] 1 # _ :: Num -> Num -> Num"));
    // src/snapshots/sel__tests__parsing-26.snap
    assert_debug_snapshot!(t(b"iterate [div 1, _ 1] 1 # _ :: Num -> Num -> Num"));
    // src/snapshots/sel__tests__parsing-27.snap
    assert_debug_snapshot!(t(
        b"iterate [div 1, add 1] 1, _ # _ :: [Num]+ -> returnof(typeof(_))"
    ));
    // src/snapshots/sel__tests__parsing-28.snap
    assert_debug_snapshot!(t(b"let {a, b} [add a b] [panic::]"));
    // src/snapshots/sel__tests__parsing-29.snap
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} h [panic::]"));
    // src/snapshots/sel__tests__parsing-30.snap
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} t [panic::]"));
    // src/snapshots/sel__tests__parsing-31.snap
    assert_debug_snapshot!(t(b"repeat 1, let {h,, t} t [panic::]"));
    // src/snapshots/sel__tests__parsing-32.snap
    assert_debug_snapshot!(t(b"let a let b a b a # note: is syntax error"));
    // src/snapshots/sel__tests__parsing-33.snap
    assert_debug_snapshot!(t(b"let {a b, c} 0"));
    // src/snapshots/sel__tests__parsing-34.snap
    assert_debug_snapshot!(t(b"[1, let 0 fst snd] 1=:a:"));
    // src/snapshots/sel__tests__parsing-35.snap
    assert_debug_snapshot!(t(b"add 1, map, flip apply {1, 2, 3}"));
    // not quite a syntax error but only out of luck:
    // ```sel
    // [let a
    //     [[let b
    //         a
    //     ]                           # b -> a
    //         [panic: unreachable:]]  # a
    // ]                               # a -> a
    //     [panic: unreachable:]       # a
    // ```
    // src/snapshots/sel__tests__parsing-36.snap
    assert_debug_snapshot!(t(b"
let a
    [let b
        a
        [panic: unreachable:]]
    [panic: unreachable:]
"));
    // src/snapshots/sel__tests__parsing-37.snap
    assert_debug_snapshot!(t(b"use :bidoof: bdf, bdf-main"));
    // src/snapshots/sel__tests__parsing-38.snap
    assert_debug_snapshot!(t(b"
def head:
    returns the first item:
    [ let {h,, t}
        h
        [panic: head on empty list:]
    ],
head {1, 2, 3}
"));
    // src/snapshots/sel__tests__parsing-39.snap
    assert_debug_snapshot!(t(b"
def sum:: [
    let {h,, t}
        #[sum t, add h] # cannot use this syntax, otherwise `sum t` is made to be `:: a -> b`
                        # note: accepting type hinting in the description would 'solve' this
        [add h [sum t]]
        0
],
{sum, _}
"));
    // src/snapshots/sel__tests__parsing-40.snap
    assert_debug_snapshot!(t(b"
def a:: [b 2],
def b:: [add 1],
a
"));
    // src/snapshots/sel__tests__parsing-41.snap
    assert_debug_snapshot!(t(b"def a:: [add a, a], a"));
    // src/snapshots/sel__tests__parsing-42.snap
    assert_debug_snapshot!(t(b"
def mre :: # a -> returnof(mre)
    [ let a
        [mre a] ],
mre
"));
}

#[test]
fn interpreting() {
    fn t(script: &[u8]) -> String {
        let mut global = Global::with_builtin();
        let source = global.registry.add_bytes("<test>", script.into());
        let result = parse::process(source, &mut global);
        let mut out = Vec::new();
        interp::run_write(&result.tree.unwrap(), &global, &mut out).unwrap();
        String::from_utf8(out).unwrap()
    }

    // src/snapshots/sel__tests__interpreting.snap
    assert_snapshot!(t(b"0"));
    // src/snapshots/sel__tests__interpreting-2.snap
    assert_snapshot!(t(b"add 1 2"));
    // src/snapshots/sel__tests__interpreting-3.snap
    assert_snapshot!(t(b"let a [add a 1], flip apply 2"));
}
