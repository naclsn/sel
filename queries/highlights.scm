;;; Highlighting for sel

(comment) @comment
(ERROR) @error

(number) @number
(string) @string

((name) @keyword
  (#match? @keyword "def"))
(name) @function

["," ";"] @punctuation.delimiter
["[" "]" "{" "}"] @punctuation.bracket

(unop) @operator.unary
(binop) @operator.binary
