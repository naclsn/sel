REWRITE IN PROGRESS of parser
    mostly to extract the typing
    parsing should return a struct representing the file (AST/CST)
        - `use` section
        - `def` section
        - script section
biggest impact of behavior is error ordering;
    before:
        - mixed syntax/resolution/type errors, but in perfect file-order, also containing `use`d file error recursively
    after potentially:
        - all syntax errors for current file
        - all resolution errors for current file
        - recursively if requested, otherwise simply "file contains error" for `use`s
        - all type errors for current file
other potential evolution (as in which would become somewhat more plausible)
    - better broken syntax recovery
    - formatter(s) (debug ast, actual `sel fmt`, maybe a version that normalizes or denoromalize, maybe a version that inlines, ...)

FiniteBoth? FiniteEither?




formatting
https://justinpombrio.net/2024/02/23/a-twist-on-Wadlers-printer.html

pattern matching (and also some formatting)
https://github.com/yorickpeterse/pattern-matching-in-rust
https://gleam.run/news/v0.33-exhaustive-gleam/
both from:
https://yorickpeterse.com/articles/how-to-write-a-code-formatter/

cargo r -- --fmt=ast {:abc:,, [{10}, uncodepoints]}, ungraphemes

env :PATH:, split ::::, each [readdir, filter [len, le 2]]
TODO: all that, but also have 'advertised types', so it's possible to stub

