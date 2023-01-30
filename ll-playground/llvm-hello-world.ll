; Copied directly from https://github.com/dfellis/llvm-hello-world
; Copied directly from the documentation
; Declare the string constant as a global constant.
@.str = private unnamed_addr constant [12 x i8] c"hello world\00"

; External declaration of the puts function
declare i32 @puts(i8* nocapture) nounwind

; Definition of main function
define i32 @main() { ; i32()*
    ; Convert [12 x i8]* to i8  *...
    %cast210 = getelementptr [12 x i8],[12 x i8]* @.str, i64 0, i64 0

    ; Call puts function to write out the string to stdout.
    call i32 @puts(i8* %cast210)
    ret i32 0
}

; Named metadata
!0 = !{i32 42, null, !"string"}
!foo = !{!0}

; llc -filetype=obj llvm-hello-world.ll -o llvm-hello-world.ll.o
; clang llvm-hello-world.ll.o -o llvm-hello-world
; (or just lli llvm-hello-world.ll)
