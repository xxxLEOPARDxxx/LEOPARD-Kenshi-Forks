?save@SaveManager@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z RVA=47b0b0 slot=6194 PID=9388
0047b0b0 push      rdi
0047b0b2 sub       rsp, 0x100
0047b0b9 mov       qword ptr [rsp + 0x20], 0xfffffffffffffffe
0047b0c2 mov       qword ptr [rsp + 0x120], rbx
0047b0ca mov       rax, qword ptr [rip + 0x1c9b2e7]
0047b0d1 xor       rax, rsp
0047b0d4 mov       qword ptr [rsp + 0xf0], rax
0047b0dc movzx     edi, r8b
0047b0e0 mov       rbx, rcx
0047b0e3 cmp       qword ptr [rip + 0x1cb7825], 0
0047b0eb jne       0x7ff7ea38b1c2
0047b0f1 cmp       dword ptr [rcx + 0xa0], 0
0047b0f8 jne       0x7ff7ea38b1c2
0047b0fe mov       dword ptr [rcx + 0xa0], 1
0047b108 add       rcx, 0xb0
0047b10f or        r9, 0xffffffffffffffff
0047b113 xor       r8d, r8d
0047b116 call      0x7ff7e9f3808d
0047b11b xor       eax, eax
0047b11d test      dil, dil
0047b120 setne     al
0047b123 mov       dword ptr [rbx + 0xa4], eax
0047b129 mov       dword ptr [rbx + 0xa8], 1
0047b133 cmp       byte ptr [rip + 0x1cb73e9], 0
0047b13a lea       rdx, [rbx + 0x78]
0047b13e jne       0x7ff7ea38b144
0047b140 lea       rdx, [rbx + 0x50]
0047b144 lea       rcx, [rbx + 0xd8]
0047b14b or        r9, 0xffffffffffffffff
0047b14f xor       r8d, r8d
0047b152 call      0x7ff7e9f3808d
0047b157 test      dil, dil
0047b15a jne       0x7ff7ea38b1c2
0047b15c lea       rdx, [rip + 0x125d1dd]
0047b163 lea       rcx, [rsp + 0x50]
0047b168 call      0x7ff7e9f210b8
0047b16d nop
0047b16e lea       rdx, [rsp + 0x28]
0047b173 mov       rcx, rax
0047b176 call      0x7ff7e9f18a3a
0047b17b nop
0047b17c xor       r8d, r8d
0047b17f mov       rdx, rax
0047b182 lea       rcx, [rip + 0x1cb7f27]
0047b189 call      0x7ff7e9f3b1ac ; ?showPlayerAMessage@GameWorld@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z
0047b18e nop
0047b18f cmp       qword ptr [rsp + 0x40], 0x10
0047b195 jb        0x7ff7ea38b1a1
0047b197 mov       rcx, qword ptr [rsp + 0x28]
0047b19c call      0x7ff7eade5628
0047b1a1 mov       qword ptr [rsp + 0x40], 0xf
0047b1aa mov       qword ptr [rsp + 0x38], 0
0047b1b3 mov       byte ptr [rsp + 0x28], 0
0047b1b8 lea       rcx, [rsp + 0x50]
0047b1bd call      0x7ff7e9f13c9c
0047b1c2 mov       rcx, qword ptr [rsp + 0xf0]
0047b1ca xor       rcx, rsp
0047b1cd call      0x7ff7eade5db0
0047b1d2 mov       rbx, qword ptr [rsp + 0x120]
0047b1da add       rsp, 0x100
0047b1e1 pop       rdi
0047b1e2 ret
0047b1e3 int3
