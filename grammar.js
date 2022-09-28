
/*
script ::= fitting {',' fitting}
comment ::= /#[^\n]*\r?\n/

fitting ::=
	| expression

expression ::=
	| name
	| literal
	| binary      ::= expression binop expression
	| unary       ::= unop expression
	| partially
	| application ::= expression {expression}
	| grouping    ::= '[' expression ']'

partially ::=
	| expression binop
	| binop expression
(* may limit partial application to literals and names only *)

literal ::=
	| number
	| string

litarray ::= (litaitem {',' litaitem})?
litaitem ::=
	| number
	| '-' number
	| number ':' number
	| '{' litarray '}'
	| /[^\-:{}]+/

name   ::= /[a-z]+/
(* having digits in identifier would enable eg. Haskell's foldr1
   -> yeah, but you can just use a naming scheme that does without
      -> yeah, but *not* having them is counter-intuitive or unusual
   -> not having them makes possible eg. `drop1` for `drop 1`
      -> dude, you seriouly trying to save 1 character?
*)
number ::= /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/
string ::= /\{([^}](?R))\}/
binop  ::= /[+,\-.\/:=_~]/
unop   ::= /[@]/
unsure ::= /[%^]/
*/
