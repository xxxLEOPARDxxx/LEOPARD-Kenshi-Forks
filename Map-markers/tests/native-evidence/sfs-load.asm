?loadGame@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z RVA=474340 slot=6168 PID=9388
00474340 mov       qword ptr [rsp + 8], rbx
00474345 push      rdi
00474346 sub       rsp, 0x50
0047434a mov       rbx, rdx
0047434d mov       rdi, rcx
00474350 call      0x7ff7e9f29835 ; ?newGame@SaveFileSystem@@QEAAXXZ
00474355 lea       r8, [rip + 0x12265f0]
0047435c lea       rcx, [rsp + 0x20]
00474361 mov       rdx, rbx
00474364 call      0x7ff7e9f32c4b
00474369 lea       rcx, [rdi + 0xf0]
00474370 mov       rdx, rax
00474373 call      0x7ff7e9f644b7
00474378 lea       rcx, [rdi + 0x120]
0047437f or        r9, 0xffffffffffffffff
00474383 xor       r8d, r8d
00474386 mov       rdx, rbx
00474389 call      0x7ff7e9f3808d
0047438e mov       rcx, rdi
00474391 mov       rbx, qword ptr [rsp + 0x60]
00474396 add       rsp, 0x50
0047439a pop       rdi
0047439b jmp       0x7ff7e9f4378f ; ?analyse@SaveFileSystem@@QEAAXXZ
004743a0 int3
