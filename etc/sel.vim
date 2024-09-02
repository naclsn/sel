fu s:sel_ft()
  setl com=:#,s::,e:: def=^\ *def isk=-,A-Z,a-z
  sy sync fromstart

  sy match   selComment  /_\|#.*/ contains=selTodo
  sy keyword selTodo     TODO FIXME XXX contained
  sy match   selNumber   /\v(0b[01]+)|(0o[0-7]+)|(0x[0-9A-Fa-f]+)|([0-9]+(\.[0-9]+)?)/
  sy region  selString   start=/:/ skip=/::/ end=/:/
  sy match   selOperator /[,=]/
  sy cluster selPlain    contains=selComment,selNumber,selString,selOperator

  sy keyword selLet      let skipwhite skipempty nextgroup=selLetWord,selLetList
  sy match   selLetWord  /\k\+/ contained
  sy region  selLetList  start=/{/ end=/}/ contains=selLetWord,@selPlain contained

  sy keyword selDef      def skipwhite skipempty nextgroup=selDefName
  sy match   selDefName  /\k\+/ skipwhite skipempty nextgroup=selDefDesc contained
  sy region  selDefDesc  start=/:/ skip=/::/ end=/:/ skipwhite skipempty nextgroup=selDefBegin contains=@Spell,selTodo contained
  sy region  selDefBegin start=/\zs/ matchgroup=selDefEnd end=/;/ contains=selDefMiss,@selPlain,selLet fold contained
  sy match   selDefMiss  /\<def\>/

  sy keyword selUse      use

  hi def link selComment  Comment
  hi def link selTodo     Todo
  hi def link selNumber   Number
  hi def link selString   String
  hi def link selOperator Operator

  hi def link selLet      Keyword
  hi def link selLetWord  Identifier

  hi def link selDef      Keyword
  hi def link selDefName  Function
  hi def link selDefDesc  SpecialComment
  hi def link selDefEnd   Keyword
  hi def link selDefMiss  Error

  hi def link selUse      Keyword
endf

aug filetypedetect
  au BufRead,BufNewFile *.sel setf sel
aug END
au FileType sel cal <SID>sel_ft()

" vim: ts=2
