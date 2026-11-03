?newGame@KingOfRenderThread@@QEAAXPEAVWidget@MyGUI@@@Z RVA=871ea0 slot=4508 PID=9388
00871ea0 push      rbx
00871ea2 sub       rsp, 0x20
00871ea6 mov       rbx, rcx
00871ea9 lea       rcx, [rip + 0x18c1200]
00871eb0 call      0x7ff7e9f37778 ; ?initialisationGameData@GameWorld@@QEAA_NXZ
00871eb5 cmp       byte ptr [rip + 0x18c1d24], 0
00871ebc jne       0x7ff7ea781ee4
00871ebe mov       byte ptr [rip + 0x18c1d1b], 1
00871ec5 call      qword ptr [rip + 0x19d5cc5]
00871ecb lea       rdx, [rip + 0x17061c6]
00871ed2 mov       rcx, rax
00871ed5 call      qword ptr [rip + 0x19d5cbd]
00871edb mov       rcx, qword ptr [rbx + 8]
00871edf call      0x7ff7e9f6204a ; ?initialiseScene@Renderer@@QEAAXXZ
00871ee4 lea       rcx, [rip + 0x18c11c5]
00871eeb call      0x7ff7e9f3c5e8 ; ?initialisation@GameWorld@@QEAA_NXZ
00871ef0 test      al, al
00871ef2 jne       0x7ff7ea781f12
00871ef4 lea       rdx, [rip + 0xeb00a5]
00871efb lea       rcx, [rip + 0xeaffde]
00871f02 mov       r8d, 0x2010
00871f08 add       rsp, 0x20
00871f0c pop       rbx
00871f0d jmp       0x7ff7e9f2f3ca
00871f12 add       rsp, 0x20
00871f16 pop       rbx
00871f17 jmp       0x7ff7e9f2ba40
00871f1c int3
