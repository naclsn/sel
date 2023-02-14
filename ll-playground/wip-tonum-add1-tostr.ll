declare i32 @getchar() nounwind
declare i32 @putchar(i32) nounwind

%Str = type {
  i8*, ; char* cur
  i32 ; int len
}

%Tostr1 = type {
  %Str
}
define void @tostr1(%Str* %this) {
entry:
  ret void
}

%Tostr0 = type {
  %Str,
  %Num* ; Num* arg
}
define void @tostr0(%Str %this, %Num %arg) {
entry:
  ret void
}
;; more or less corresponds to `::stream`
;; except the result is put in the struct
define void @tostr0_next(%Str* %this) {
entry:
  ret void
}
define i1 @tostr0_end(%Str* %this) {
entry:
  ret i1 1
}

; say, `tonum, add 1, tostr`
define i32 @main() {
  ; allocate for in: Input
  ; pass to next-in-chain

  ; `tonum`
  ; allocate for tonum_res: Num
  ; call tonum(tonum_res, in)
  ; pass to next-in-chain

  ; `add 1`
  ; allocate for add_1_res: Num
  ; call add_1(add_1_res, tonum_res)
  ; pass to next-in-chain

  ; `tostr`
  ; allocate for tostr_res: Str
  ; call tostr(tostr_res, add_1_res)
  ; pass to next-in-chain

  ; ::entire

  ret i32 42
}

; const 1, repeat, map tostr, join :-:
