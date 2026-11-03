?getSingleton@SaveFileSystem@@SAPEAV1@XZ RVA=37dd80 slot=6158 PID=9388
0037dd80 sub       rsp, 0x38
0037dd84 mov       qword ptr [rsp + 0x20], 0xfffffffffffffffe
0037dd8d mov       eax, dword ptr [rip + 0x1dafe81]
0037dd93 test      al, 1
0037dd95 jne       0x7ff7ea28ddc6
0037dd97 or        eax, 1
0037dd9a mov       dword ptr [rip + 0x1dafe74], eax
0037dda0 mov       ecx, 0x318
0037dda5 call      0x7ff7eade563a
0037ddaa mov       qword ptr [rsp + 0x40], rax
0037ddaf test      rax, rax
0037ddb2 je        0x7ff7ea28ddbd
0037ddb4 mov       rcx, rax
0037ddb7 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
0037ddbc nop
0037ddbd mov       qword ptr [rip + 0x1dafe44], rax
0037ddc4 jmp       0x7ff7ea28ddcd
0037ddc6 mov       rax, qword ptr [rip + 0x1dafe3b]
0037ddcd add       rsp, 0x38
0037ddd1 ret
0037ddd2 int3
