declare i32 @getchar() nounwind
declare i32 @putchar(i32) nounwind

define void @hey() {
entry:
  %_a = alloca i32
  store i32 9, i32* %_a
  br label %loop

loop:
  %a = load i32, i32* %_a

  %char = add i32 %a, 48 ; '0'
  call i32 @putchar(i32 %char)
  call i32 @putchar(i32 10) ; '\n'

  %amm = sub i32 %a, 1
  store i32 %amm, i32* %_a
  %zero = icmp eq i32 0, %amm
  br i1 %zero, label %break, label %loop

break:
  ret void
}

define i32 @main() {


output_loop_enter:

output_loop_iter:
  %output_arg_str_bufstart = ;
  call void @putbuff(i32* %output_arg_str_bufstart, i32* %output_arg_str_buflen)

output_loop_check:
  %output_loop_cond = ;
  br i1 %output_loop_cond, label %output_loop_leave, label %output_loop_enter

output_loop_leave:

  ret i32 0
}
