declare i32 @getchar() nounwind
declare i32 @putchar(i32) nounwind

define i32 @tostr(i8* %_r, i32 %num) {
entry:
  %_num = alloca i32
  store i32 %num, i32* %_num

  %_at = alloca i32
  store i32 0, i32* %_at
  br label %again

again:
  %curr_num = load i32, i32* %_num
  %digit = srem i32 %curr_num, 10

  %at = load i32, i32* %_at
  %_r.at = getelementptr i8, i8* %_r, i32 %at
  %wchar = add i32 %digit, 48 ; '0'
  %char = trunc i32 %wchar to i8
  store i8 %char, i8* %_r.at

  %atpp = add i32 %at, 1
  store i32 %atpp, i32* %_at

  %next_num = sdiv i32 %curr_num, 10
  store i32 %next_num, i32* %_num

  %isend = icmp eq i32 %next_num, 0
  br i1 %isend, label %reverse, label %again

reverse:
  %at2 = load i32, i32* %_at
  %_r.tail = getelementptr i8, i8* %_r, i32 %at2

  %ta_tmp = sub i32 %atpp, 1
  %ta = sub i32 %ta_tmp, %at2
  %_r.head = getelementptr i8, i8* %_r, i32 %ta

  %swp_tmp.tail = load i8, i8* %_r.tail
  %swp_tmp.head = load i8, i8* %_r.head

  store i8 %swp_tmp.tail, i8* %_r.head
  store i8 %swp_tmp.head, i8* %_r.tail

  %atmm = sub i32 %at2, 1
  store i32 %atmm, i32* %_at

  %at_half = lshr i32 %ta_tmp, 1
  %isend2 = icmp sle i32 %atmm, %at_half
  br i1 %isend2, label %end, label %reverse

end:
  ret i32 %atpp
}

define i32 @main() {
  %n = call i32 @getchar()

  %_s = alloca i8, i8 8
  %len = call i32 @tostr(i8* %_s, i32 %n)

  %_at = alloca i32
  store i32 0, i32* %_at
  br label %next

next:
  %at = load i32, i32* %_at
  %_s.at = getelementptr i8, i8* %_s, i32 %at
  %char = load i8, i8* %_s.at
  %wchar = zext i8 %char to i32
  call i32 @putchar(i32 %wchar)

  %atpp = add i32 %at, 1
  store i32 %atpp, i32* %_at

  %isend = icmp eq i32 %len, %atpp
  br i1 %isend, label %done, label %next

done:
  call i32 @putchar(i32 10) ; '\n'
  ret i32 0
}
