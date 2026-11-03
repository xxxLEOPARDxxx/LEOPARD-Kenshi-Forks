?newGame@SaveFileSystem@@QEAAXXZ RVA=470d00 slot=6167 PID=9388
00470d00 mov       qword ptr [rsp + 8], rbx
00470d05 mov       qword ptr [rsp + 0x10], rsi
00470d0a push      rdi
00470d0b sub       rsp, 0x50
00470d0f xor       esi, esi
00470d11 mov       rdi, rcx
00470d14 cmp       qword ptr [rcx + 0x158], rsi
00470d1b je        0x7ff7ea380d59
00470d1d lea       rdx, [rcx + 0x148]
00470d24 lea       rcx, [rsp + 0x20]
00470d29 or        r9, 0xffffffffffffffff
00470d2d xor       r8d, r8d
00470d30 mov       qword ptr [rsp + 0x38], 0xf
00470d39 mov       qword ptr [rsp + 0x30], rsi
00470d3e mov       byte ptr [rsp + 0x20], sil
00470d43 call      0x7ff7e9f3808d
00470d48 lea       rcx, [rdi + 0x108]
00470d4f lea       rdx, [rsp + 0x20]
00470d54 call      0x7ff7e9f644b7
00470d59 lea       rax, [rdi + 0x148]
00470d60 cmp       qword ptr [rax + 0x18], 0x10
00470d65 mov       qword ptr [rax + 0x10], rsi
00470d69 jb        0x7ff7ea380d6e
00470d6b mov       rax, qword ptr [rax]
00470d6e mov       byte ptr [rax], sil
00470d71 lea       rax, [rdi + 0x120]
00470d78 cmp       qword ptr [rax + 0x18], 0x10
00470d7d mov       qword ptr [rax + 0x10], rsi
00470d81 jb        0x7ff7ea380d86
00470d83 mov       rax, qword ptr [rax]
00470d86 mov       byte ptr [rax], sil
00470d89 mov       dword ptr [rdi + 0xf8], esi
00470d8f mov       rdx, qword ptr [rdi + 0x90]
00470d96 mov       rdx, qword ptr [rdx + 8]
00470d9a lea       rcx, [rdi + 0x88]
00470da1 call      0x7ff7e9f1c74d
00470da6 mov       r11, qword ptr [rdi + 0x90]
00470dad mov       qword ptr [r11 + 8], r11
00470db1 mov       rax, qword ptr [rdi + 0x90]
00470db8 mov       qword ptr [rax], rax
00470dbb mov       rax, qword ptr [rdi + 0x90]
00470dc2 mov       qword ptr [rax + 0x10], rax
00470dc6 mov       qword ptr [rdi + 0x98], rsi
00470dcd cmp       qword ptr [rdi + 0xd0], rsi
00470dd4 je        0x7ff7ea380e1c
00470dd6 mov       rcx, qword ptr [rdi + 0xc8]
00470ddd mov       rax, qword ptr [rdi + 0xe8]
00470de4 xor       r8d, r8d
00470de7 lea       rdx, [rax + rcx*8]
00470deb lea       rcx, [rdi + 0xb0]
00470df2 call      0x7ff7e9f3526b
00470df7 mov       r11, qword ptr [rdi + 0xe8]
00470dfe mov       rax, qword ptr [rdi + 0xc8]
00470e05 lea       rcx, [r11 + rax*8]
00470e09 cmp       r11, rcx
00470e0c je        0x7ff7ea380e1c
00470e0e nop
00470e10 mov       qword ptr [r11], rsi
00470e13 add       r11, 8
00470e17 cmp       r11, rcx
00470e1a jne       0x7ff7ea380e10
00470e1c mov       rbx, qword ptr [rsp + 0x60]
00470e21 mov       rsi, qword ptr [rsp + 0x68]
00470e26 add       rsp, 0x50
00470e2a pop       rdi
00470e2b ret
00470e2c int3
