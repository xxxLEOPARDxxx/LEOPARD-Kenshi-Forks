option casemap:none
EXTERN KEO_TestNativeLower:PROC
EXTERN KEO_TestCompleteIndex:PROC
PUBLIC KEO_TestSaveDriver
PUBLIC KEO_TestOtherDriver
PUBLIC KEO_TestParserAfter
PUBLIC KEO_TestOuterAfter
PUBLIC KEO_TestLowerPivot
PUBLIC KEO_TestLowerAfter
.code
KEO_TestSaveDriver PROC FRAME
 sub rsp,28h
 .allocstack 28h
 .endprolog
 call KEO_TestOuterStage
KEO_TestParserAfter LABEL BYTE
 add rsp,28h
 ret
KEO_TestSaveDriver ENDP
KEO_TestOtherDriver PROC FRAME
 sub rsp,28h
 .allocstack 28h
 .endprolog
 call KEO_TestOuterStage
 add rsp,28h
 ret
KEO_TestOtherDriver ENDP
KEO_TestOuterStage PROC FRAME
 push rdi
 .pushreg rdi
 push r12
 .pushreg r12
 push r13
 .pushreg r13
 sub rsp,50h
 .allocstack 50h
 .endprolog
 mov rdi,rcx
 mov r12,rdx
 call KEO_TestNativeLower
 mov [rsp+70h],rax
 mov rcx,rdi
 mov rdx,r12
 call KEO_TestIndexStage
KEO_TestOuterAfter LABEL BYTE
 add rsp,50h
 pop r13
 pop r12
 pop rdi
 ret
KEO_TestOuterStage ENDP
KEO_TestIndexStage PROC FRAME
 push rbp
 .pushreg rbp
 push rdi
 .pushreg rdi
 push r12
 .pushreg r12
 sub rsp,140h
 .allocstack 140h
 .endprolog
 mov [rsp+30h],rcx
 mov [rsp+38h],rdx
 mov rdi,rdx
KEO_TestLowerPivot LABEL BYTE
 call KEO_TestNativeLower
KEO_TestLowerAfter LABEL BYTE
 mov r8,rax
 mov rcx,[rsp+30h]
 mov rdx,[rsp+38h]
 call KEO_TestCompleteIndex
 add rsp,140h
 pop r12
 pop rdi
 pop rbp
 ret
KEO_TestIndexStage ENDP
END
