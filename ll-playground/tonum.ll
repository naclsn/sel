declare i32 @getchar() nounwind
declare i32 @putchar(i32) nounwind

define i32 @tonum() {
entry:
  %_r = alloca i32, align 4
  store i32 0, i32* %_r, align 4
  br label %next

next:
  %char = call i32 @getchar()
  %num = sub i32 %char, 48 ; '0'

  %isnum.ge = icmp sge i32 %num, 0
  %isnum.le = icmp sle i32 %num, 9
  %isnum = and i1 %isnum.ge, %isnum.le
  br i1 %isnum, label %accumulate, label %break

accumulate:
  %r.x = load i32, i32* %_r, align 4
  %r.x0 = mul i32 %r.x, 10
  %r.xy = add i32 %r.x0, %num
  store i32 %r.xy, i32* %_r, align 4
  br label %next

break:
  %r = load i32, i32* %_r, align 4
  ret i32 %r
}

define i32 @main() {
  %r = call i32 @tonum()
  ret i32 %r
}
