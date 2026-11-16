; Offline seven-argument Win64 frame matching native AddSaved.
; The caller-return/pivot-return difference is the exact native 8A06h.
option casemap:none
EXTERN KEO_TestAssignSlot:QWORD
EXTERN KEO_TestAssignmentResult:QWORD
EXTERN KEO_TestAltSlot:QWORD
EXTERN KEO_TestUseAlt:BYTE
EXTERN KEO_TestCopyStates:PROC
PUBLIC KEO_TestSavedDriver
PUBLIC KEO_TestParserReturn
PUBLIC KEO_TestSavedPivot
PUBLIC KEO_TestSavedAfter
PUBLIC KEO_TestSavedStage
.code
KEO_TestSavedDriver PROC FRAME
 sub rsp,48h
 .allocstack 48h
 .endprolog
 mov rax,[rsp+70h]
 mov [rsp+20h],rax
 mov rax,[rsp+78h]
 mov [rsp+28h],rax
 mov rax,[rsp+80h]
 mov [rsp+30h],rax
 align 8
 nop
 nop
 call KEO_TestSavedStage
KEO_TestParserReturn LABEL BYTE
 add rsp,48h
 ret
KEO_TestSavedDriver ENDP
KEO_TestSavedStage PROC FRAME
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
 lea rcx,[rdi+0ch]
 mov rdx,[rsp+98h]
 cmp KEO_TestUseAlt,0
 jne KEO_TestAltPivot
 jmp NEAR PTR KEO_TestSavedPivot
 db (8a00h-($-KEO_TestParserReturn)) dup(0cch)
KEO_TestSavedPivot LABEL BYTE
 call QWORD PTR [KEO_TestAssignSlot]
KEO_TestSavedAfter LABEL BYTE
 mov KEO_TestAssignmentResult,rax
 lea rcx,[rdi+50h]
 mov rdx,[rsp+0a0h]
 call KEO_TestCopyStates
 add rsp,50h
 pop r13
 pop r12
 pop rdi
 ret
KEO_TestAltPivot:
 call QWORD PTR [KEO_TestAltSlot]
 jmp NEAR PTR KEO_TestSavedAfter
KEO_TestSavedStage ENDP
END
