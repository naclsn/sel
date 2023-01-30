declare i32 @getchar() nounwind
declare i32 @putchar(i32) nounwind

define i32 @main() {
entry:
  br label %next

next:
  %char = call i32 @getchar()
  %isend = icmp eq i32 %char, -1 ; EOF
  br i1 %isend, label %break, label %continue

continue:
  call i32 @putchar(i32 %char)
  br label %next

break:
  ret i32 0
}
