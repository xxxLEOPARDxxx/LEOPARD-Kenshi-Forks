?loadGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z RVA=373dc0 slot=6213 PID=9388
00373dc0 push      rbp
00373dc2 push      rsi
00373dc3 push      rdi
00373dc4 push      r12
00373dc6 push      r13
00373dc8 push      r14
00373dca push      r15
00373dcc lea       rbp, [rsp - 0x340]
00373dd4 sub       rsp, 0x440
00373ddb mov       qword ptr [rbp - 0x58], 0xfffffffffffffffe
00373de3 mov       qword ptr [rsp + 0x498], rbx
00373deb mov       rax, qword ptr [rip + 0x1da25c6]
00373df2 xor       rax, rsp
00373df5 mov       qword ptr [rbp + 0x330], rax
00373dfc mov       rbx, r8
00373dff mov       rsi, rdx
00373e02 mov       r13, rcx
00373e05 xor       edi, edi
00373e07 mov       dword ptr [rsp + 0x38], edi
00373e0b call      0x7ff7e9f561af ; ?saveExists@SaveManager@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
00373e10 test      al, al
00373e12 jne       0x7ff7ea283e1c
00373e14 lea       eax, [rdi + 1]
00373e17 jmp       0x7ff7ea284f8c
00373e1c mov       r8, rbx
00373e1f lea       rdx, [rip + 0x134fc72]
00373e26 lea       rcx, [rbp - 0x28]
00373e2a call      0x7ff7e9f3851f
00373e2f nop
00373e30 lea       r8, [rip + 0x130ef61]
00373e37 mov       rdx, rax
00373e3a lea       rcx, [rbp - 0x50]
00373e3e call      0x7ff7e9f42d6c
00373e43 nop
00373e44 mov       edx, 2
00373e49 mov       rcx, rax
00373e4c call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
00373e51 nop
00373e52 cmp       qword ptr [rbp - 0x38], 0x10
00373e57 jb        0x7ff7ea283e62
00373e59 mov       rcx, qword ptr [rbp - 0x50]
00373e5d call      0x7ff7eade5628
00373e62 mov       qword ptr [rbp - 0x38], 0xf
00373e6a mov       qword ptr [rbp - 0x40], rdi
00373e6e mov       byte ptr [rbp - 0x50], 0
00373e72 cmp       qword ptr [rbp - 0x10], 0x10
00373e77 jb        0x7ff7ea283e82
00373e79 mov       rcx, qword ptr [rbp - 0x28]
00373e7d call      0x7ff7eade5628
00373e82 lea       rcx, [rip + 0x1dbf227]
00373e89 call      0x7ff7e9f51c31 ; ?resetGame@GameWorld@@QEAAXXZ
00373e8e lea       rcx, [rip + 0x1dbf53b]
00373e95 call      0x7ff7e9f24556 ; ?clearAndDestroy@GameDataContainer@@QEAAXXZ
00373e9a mov       r8, rbx
00373e9d mov       rdx, rsi
00373ea0 lea       rcx, [rbp - 0x50]
00373ea4 call      0x7ff7e9f36945
00373ea9 mov       rbx, rax
00373eac mov       ecx, dword ptr [rip + 0x1db9d62]
00373eb2 test      cl, 1
00373eb5 jne       0x7ff7ea283eea
00373eb7 or        ecx, 1
00373eba mov       dword ptr [rip + 0x1db9d54], ecx
00373ec0 mov       ecx, 0x318
00373ec5 call      0x7ff7eade563a
00373eca mov       qword ptr [rsp + 0x58], rax
00373ecf test      rax, rax
00373ed2 je        0x7ff7ea283ede
00373ed4 mov       rcx, rax
00373ed7 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00373edc jmp       0x7ff7ea283ee1
00373ede mov       rax, rdi
00373ee1 mov       qword ptr [rip + 0x1db9d20], rax
00373ee8 jmp       0x7ff7ea283ef1
00373eea mov       rax, qword ptr [rip + 0x1db9d17]
00373ef1 mov       rdx, rbx
00373ef4 mov       rcx, rax
00373ef7 call      0x7ff7e9f46cfa ; ?loadGame@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00373efc nop
00373efd cmp       qword ptr [rbp - 0x38], 0x10
00373f02 jb        0x7ff7ea283f0d
00373f04 mov       rcx, qword ptr [rbp - 0x50]
00373f08 call      0x7ff7eade5628
00373f0d mov       qword ptr [rbp + 0x18], 0xf
00373f15 mov       qword ptr [rbp + 0x10], rdi
00373f19 mov       byte ptr [rbp], 0
00373f1d mov       r14d, 0xa
00373f23 mov       r8d, r14d
00373f26 lea       rdx, [rip + 0x134f263]
00373f2d lea       rcx, [rbp]
00373f31 call      0x7ff7e9f5d7c5
00373f36 nop
00373f37 mov       eax, dword ptr [rip + 0x1db9cd7]
00373f3d test      al, 1
00373f3f jne       0x7ff7ea283f74
00373f41 or        eax, 1
00373f44 mov       dword ptr [rip + 0x1db9cca], eax
00373f4a mov       ecx, 0x318
00373f4f call      0x7ff7eade563a
00373f54 mov       qword ptr [rsp + 0x58], rax
00373f59 test      rax, rax
00373f5c je        0x7ff7ea283f68
00373f5e mov       rcx, rax
00373f61 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00373f66 jmp       0x7ff7ea283f6b
00373f68 mov       rax, rdi
00373f6b mov       qword ptr [rip + 0x1db9c96], rax
00373f72 jmp       0x7ff7ea283f7b
00373f74 mov       rax, qword ptr [rip + 0x1db9c8d]
00373f7b lea       r8, [rbp]
00373f7f lea       rdx, [rbp + 0x260]
00373f86 mov       rcx, rax
00373f89 call      0x7ff7e9f5d725 ; ?readFile@SaveFileSystem@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
00373f8e nop
00373f8f cmp       qword ptr [rbp + 0x18], 0x10
00373f94 jb        0x7ff7ea283f9f
00373f96 mov       rcx, qword ptr [rbp]
00373f9a call      0x7ff7eade5628
00373f9f mov       qword ptr [rbp + 0x18], 0xf
00373fa7 mov       qword ptr [rbp + 0x10], rdi
00373fab mov       byte ptr [rbp], 0
00373faf mov       qword ptr [rbp - 0x38], 0xf
00373fb7 mov       qword ptr [rbp - 0x40], rdi
00373fbb mov       byte ptr [rbp - 0x50], 0
00373fbf or        r12, 0xffffffffffffffff
00373fc3 mov       r9, r12
00373fc6 xor       r8d, r8d
00373fc9 lea       rdx, [rbp + 0x260]
00373fd0 lea       rcx, [rbp - 0x50]
00373fd4 call      0x7ff7e9f3808d
00373fd9 lea       r11, [rip + 0x1dbef50]
00373fe0 mov       qword ptr [rsp + 0x20], r11
00373fe5 xor       r9d, r9d
00373fe8 mov       r8b, 1
00373feb lea       rdx, [rbp - 0x50]
00373fef lea       rcx, [rip + 0x1dbf3da]
00373ff6 call      0x7ff7e9f47a8d ; ?loadGameDataReturn@GameDataContainer@@QEAA_NV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N1PEAVSerialisable@@@Z
00373ffb mov       r11, qword ptr [rip + 0x1dbf95e]
00374002 mov       byte ptr [r11 + 8], 1
00374007 lea       rax, [rip + 0x130a4d2]
0037400e mov       qword ptr [rsp + 0x40], rax
00374013 mov       dword ptr [rsp + 0x48], edi
00374017 mov       dword ptr [rsp + 0x4c], r14d
0037401c mov       qword ptr [rsp + 0x50], rdi
00374021 mov       eax, 8
00374026 mul       r14
00374029 cmovo     rax, r12
0037402d mov       rcx, rax
00374030 call      0x7ff7eade5634
00374035 mov       qword ptr [rsp + 0x50], rax
0037403a lea       r8d, [r12 + 0x39]
0037403f lea       rdx, [rsp + 0x40]
00374044 lea       rcx, [rip + 0x1dbf385]
0037404b call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00374050 mov       r15, rdi
00374053 cmp       dword ptr [rsp + 0x48], r15d
00374058 jbe       0x7ff7ea284071
0037405a mov       rax, qword ptr [rsp + 0x50]
0037405f mov       r15, qword ptr [rax]
00374062 mov       rdx, r15
00374065 mov       rcx, qword ptr [rip + 0x1dbb354]
0037406c call      0x7ff7e9f1352b
00374071 lea       rdx, [rip + 0x1dbf358]
00374078 lea       rcx, [rip + 0x1dbf031]
0037407f call      0x7ff7e9f38c68 ; ?initialiseNewGameWorld@GameWorld@@QEAAXPEAVGameDataManager@@@Z
00374084 xor       edx, edx
00374086 mov       rcx, qword ptr [rip + 0x1dbf013]
0037408d call      0x7ff7e9f1f52e ; ?loadLevelData@TownList@@QEAAX_N@Z
00374092 lea       rdx, [rip + 0x1dbf337]
00374099 mov       rcx, qword ptr [rip + 0x1dbf000]
003740a0 call      0x7ff7e9f60e93 ; ?loadAllTowns_worldInit@TownList@@QEAAXPEAVGameDataContainer@@@Z
003740a5 xor       r8d, r8d
003740a8 lea       rdx, [rip + 0x1dbf321]
003740af mov       rcx, qword ptr [rip + 0x1dbefea]
003740b6 call      0x7ff7e9f46c91 ; ?loadState@TownList@@QEAAXPEAVGameDataContainer@@_N@Z
003740bb mov       rcx, qword ptr [rip + 0x1dbefde]
003740c2 call      0x7ff7e9f3c381 ; ?listUniqueTownsFromPlayerOwnership@TownList@@QEAAXXZ
003740c7 mov       r11, qword ptr [rip + 0x1dbefd2]
003740ce cmp       dword ptr [r11 + 0x58], 0
003740d3 jne       0x7ff7ea28411b
003740d5 mov       qword ptr [rbp - 0x10], 0xf
003740dd mov       qword ptr [rbp - 0x18], rdi
003740e1 mov       byte ptr [rbp - 0x28], 0
003740e5 mov       r8d, 0x28
003740eb lea       rdx, [rip + 0x134f96e]
003740f2 lea       rcx, [rbp - 0x28]
003740f6 call      0x7ff7e9f5d7c5
003740fb nop
003740fc mov       edx, 3
00374101 lea       rcx, [rbp - 0x28]
00374105 call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0037410a nop
0037410b cmp       qword ptr [rbp - 0x10], 0x10
00374110 jb        0x7ff7ea28411b
00374112 mov       rcx, qword ptr [rbp - 0x28]
00374116 call      0x7ff7eade5628
0037411b test      r15, r15
0037411e je        0x7ff7ea284206
00374124 mov       qword ptr [rbp - 0x10], 0xf
0037412c mov       qword ptr [rbp - 0x18], rdi
00374130 mov       byte ptr [rbp - 0x28], 0
00374134 mov       r8d, 7
0037413a lea       rdx, [rip + 0x13354a7]
00374141 lea       rcx, [rbp - 0x28]
00374145 call      0x7ff7e9f5d7c5
0037414a nop
0037414b lea       rdx, [rbp - 0x28]
0037414f lea       rcx, [r15 + 0x138]
00374156 call      0x7ff7e9f11aeb
0037415b mov       r9, r12
0037415e xor       r8d, r8d
00374161 lea       rdx, [rax + 0x28]
00374165 lea       rcx, [r13 + 0x28]
00374169 call      0x7ff7e9f3808d
0037416e nop
0037416f cmp       qword ptr [rbp - 0x10], 0x10
00374174 jb        0x7ff7ea28417f
00374176 mov       rcx, qword ptr [rbp - 0x28]
0037417a call      0x7ff7eade5628
0037417f lea       rdx, [r13 + 0x28]
00374183 mov       rcx, r13
00374186 call      0x7ff7e9f30fc7 ; ?versionCode@SaveManager@@QEAAIAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
0037418b mov       dword ptr [rsp + 0x58], eax
0037418f mov       rdx, r15
00374192 mov       rcx, qword ptr [rip + 0x1dbf497]
00374199 mov       rcx, qword ptr [rcx + 0x30]
0037419d call      0x7ff7e9f31567 ; ?load@CameraClass@@QEAAXPEAVGameData@@@Z
003741a2 mov       qword ptr [rbp - 0x10], 0xf
003741aa mov       qword ptr [rbp - 0x18], rdi
003741ae mov       byte ptr [rbp - 0x28], 0
003741b2 mov       r8d, 0xd
003741b8 lea       rdx, [rip + 0x134f891]
003741bf lea       rcx, [rbp - 0x28]
003741c3 call      0x7ff7e9f5d7c5
003741c8 nop
003741c9 lea       rdx, [rbp - 0x28]
003741cd lea       rcx, [r15 + 0x138]
003741d4 call      0x7ff7e9f11aeb
003741d9 lea       rdx, [rax + 0x28]
003741dd mov       rcx, qword ptr [rip + 0x1dbf44c]
003741e4 add       rcx, 8
003741e8 mov       r9, r12
003741eb xor       r8d, r8d
003741ee call      0x7ff7e9f3808d
003741f3 nop
003741f4 cmp       qword ptr [rbp - 0x10], 0x10
003741f9 jb        0x7ff7ea28420e
003741fb mov       rcx, qword ptr [rbp - 0x28]
003741ff call      0x7ff7eade5628
00374204 jmp       0x7ff7ea28420e
00374206 mov       eax, dword ptr [rsp + 0x58]
0037420a mov       dword ptr [rsp + 0x58], eax
0037420e mov       rdx, qword ptr [rip + 0x1dbf41b]
00374215 cmp       qword ptr [rdx + 0x18], 0
0037421a jne       0x7ff7ea28434b
00374220 mov       qword ptr [rbp + 0x18], 0xf
00374228 mov       qword ptr [rbp + 0x10], rdi
0037422c mov       byte ptr [rbp], 0
00374230 mov       r8d, 0x11
00374236 lea       rdx, [rip + 0x134efeb]
0037423d lea       rcx, [rbp]
00374241 call      0x7ff7e9f5d7c5
00374246 nop
00374247 lea       rdx, [rbp]
0037424b lea       rcx, [rip + 0x1dbee7e]
00374252 call      0x7ff7e9f24790 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00374257 mov       rbx, rax
0037425a cmp       qword ptr [rbp + 0x18], 0x10
0037425f jb        0x7ff7ea28426a
00374261 mov       rcx, qword ptr [rbp]
00374265 call      0x7ff7eade5628
0037426a mov       qword ptr [rbp + 0x18], 0xf
00374272 mov       qword ptr [rbp + 0x10], rdi
00374276 mov       byte ptr [rbp], 0
0037427a test      rbx, rbx
0037427d je        0x7ff7ea2842ad
0037427f mov       qword ptr [rbp - 0x10], 0xf
00374287 mov       qword ptr [rbp - 0x18], rdi
0037428b mov       byte ptr [rbp - 0x28], 0
0037428f lea       rdx, [rbx + 0x28]
00374293 mov       r9, r12
00374296 xor       r8d, r8d
00374299 lea       rcx, [rbp - 0x28]
0037429d call      0x7ff7e9f3808d
003742a2 lea       rdx, [rbp - 0x28]
003742a6 mov       ebx, 1
003742ab jmp       0x7ff7ea2842dd
003742ad lea       rdx, [rip + 0x134ef64]
003742b4 lea       rcx, [rbp + 0x290]
003742bb call      0x7ff7e9f210b8
003742c0 nop
003742c1 mov       dword ptr [rsp + 0x38], 2
003742c9 lea       rdx, [rbp - 0x50]
003742cd mov       rcx, rax
003742d0 call      0x7ff7e9f18a3a
003742d5 mov       rdx, rax
003742d8 mov       ebx, 6
003742dd mov       dword ptr [rsp + 0x38], ebx
003742e1 mov       rcx, qword ptr [rip + 0x1dbf348]
003742e8 add       rcx, 8
003742ec call      0x7ff7e9f121a3
003742f1 nop
003742f2 test      bl, 4
003742f5 je        0x7ff7ea28431a
003742f7 and       ebx, 0xfffffffb
003742fa cmp       qword ptr [rbp - 0x38], 0x10
003742ff jb        0x7ff7ea28430a
00374301 mov       rcx, qword ptr [rbp - 0x50]
00374305 call      0x7ff7eade5628
0037430a mov       qword ptr [rbp - 0x38], 0xf
00374312 mov       qword ptr [rbp - 0x40], rdi
00374316 mov       byte ptr [rbp - 0x50], 0
0037431a test      bl, 2
0037431d je        0x7ff7ea28432f
0037431f and       ebx, 0xfffffffd
00374322 lea       rcx, [rbp + 0x290]
00374329 call      0x7ff7e9f13c9c
0037432e nop
0037432f test      bl, 1
00374332 je        0x7ff7ea284344
00374334 cmp       qword ptr [rbp - 0x10], 0x10
00374339 jb        0x7ff7ea284344
0037433b mov       rcx, qword ptr [rbp - 0x28]
0037433f call      0x7ff7eade5628
00374344 mov       rdx, qword ptr [rip + 0x1dbf2e5]
0037434b mov       rcx, qword ptr [rdx + 0x2a0]
00374352 add       rdx, 8
00374356 add       rcx, 0x1a8
0037435d mov       r9, r12
00374360 xor       r8d, r8d
00374363 call      0x7ff7e9f3808d
00374368 mov       dword ptr [rsp + 0x48], edi
0037436c mov       r8d, 0x1c
00374372 lea       rdx, [rsp + 0x40]
00374377 lea       rcx, [rip + 0x1dbf052]
0037437e call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00374383 cmp       dword ptr [rsp + 0x48], 0
00374388 je        0x7ff7ea28439e
0037438a mov       rax, qword ptr [rsp + 0x50]
0037438f mov       rdx, qword ptr [rax]
00374392 mov       rcx, qword ptr [rip + 0x1dbecff]
00374399 call      0x7ff7e9f3211a
0037439e mov       qword ptr [rbp - 0x10], 0xf
003743a6 mov       qword ptr [rbp - 0x18], rdi
003743aa mov       byte ptr [rbp - 0x28], 0
003743ae mov       r8, r14
003743b1 lea       rdx, [rip + 0x134f688]
003743b8 lea       rcx, [rbp - 0x28]
003743bc call      0x7ff7e9f5d7c5
003743c1 nop
003743c2 mov       r8d, 0x28
003743c8 lea       rdx, [rbp - 0x28]
003743cc lea       rcx, [rip + 0x1dbeffd]
003743d3 call      0x7ff7e9f51aa6 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4itemType@@@Z
003743d8 mov       rsi, rax
003743db cmp       qword ptr [rbp - 0x10], 0x10
003743e0 jb        0x7ff7ea2843eb
003743e2 mov       rcx, qword ptr [rbp - 0x28]
003743e6 call      0x7ff7eade5628
003743eb mov       qword ptr [rbp - 0x10], 0xf
003743f3 mov       qword ptr [rbp - 0x18], rdi
003743f7 mov       byte ptr [rbp - 0x28], 0
003743fb test      rsi, rsi
003743fe je        0x7ff7ea284564
00374404 mov       qword ptr [rbp - 0x38], 0xf
0037440c mov       qword ptr [rbp - 0x40], rdi
00374410 mov       byte ptr [rbp - 0x50], 0
00374414 mov       r8d, 5
0037441a lea       rdx, [rip + 0x1329e83]
00374421 lea       rcx, [rbp - 0x50]
00374425 call      0x7ff7e9f5d7c5
0037442a nop
0037442b lea       rcx, [rsi + 0x178]
00374432 lea       rdx, [rbp - 0x50]
00374436 call      0x7ff7e9f580fe
0037443b mov       r12d, dword ptr [rax + 0x28]
0037443f cmp       qword ptr [rbp - 0x38], 0x10
00374444 jb        0x7ff7ea28444f
00374446 mov       rcx, qword ptr [rbp - 0x50]
0037444a call      0x7ff7eade5628
0037444f lea       rax, [rip + 0x130fe7a]
00374456 mov       qword ptr [rbp], rax
0037445a mov       qword ptr [rbp + 8], 0xb
00374462 mov       dword ptr [rbp + 0x14], edi
00374465 mov       qword ptr [rsp + 0x78], rax
0037446a mov       qword ptr [rbp - 0x80], 0xb
00374472 mov       dword ptr [rbp - 0x74], edi
00374475 mov       ebx, edi
00374477 test      r12d, r12d
0037447a jle       0x7ff7ea284564
00374480 xor       r8d, r8d
00374483 mov       dword ptr [rsp + 0x20], edi
00374487 mov       r9b, 0x20
0037448a mov       edx, ebx
0037448c lea       rcx, [rbp - 0x50]
00374490 call      qword ptr [rip + 0x1ed1c4a]
00374496 nop
00374497 mov       r8, rax
0037449a lea       rdx, [rbp]
0037449e mov       rcx, rsi
003744a1 call      0x7ff7e9f14aa7 ; ?getHandle@GameData@@QEAA_NAEAVhand@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
003744a6 nop
003744a7 cmp       qword ptr [rbp - 0x38], 0x10
003744ac jb        0x7ff7ea2844b7
003744ae mov       rcx, qword ptr [rbp - 0x50]
003744b2 call      0x7ff7eade5628
003744b7 xor       r8d, r8d
003744ba mov       dword ptr [rsp + 0x20], edi
003744be mov       r9b, 0x20
003744c1 mov       edx, ebx
003744c3 lea       rcx, [rbp + 0x28]
003744c7 call      qword ptr [rip + 0x1ed1c13]
003744cd nop
003744ce mov       r8, rax
003744d1 lea       rdx, [rip + 0x134cc60]
003744d8 lea       rcx, [rbp - 0x50]
003744dc call      0x7ff7e9f623dd
003744e1 nop
003744e2 mov       r8, rax
003744e5 lea       rdx, [rsp + 0x78]
003744ea mov       rcx, rsi
003744ed call      0x7ff7e9f14aa7 ; ?getHandle@GameData@@QEAA_NAEAVhand@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
003744f2 nop
003744f3 cmp       qword ptr [rbp - 0x38], 0x10
003744f8 jb        0x7ff7ea284503
003744fa mov       rcx, qword ptr [rbp - 0x50]
003744fe call      0x7ff7eade5628
00374503 mov       qword ptr [rbp - 0x38], 0xf
0037450b mov       qword ptr [rbp - 0x40], rdi
0037450f mov       byte ptr [rbp - 0x50], 0
00374513 cmp       qword ptr [rbp + 0x40], 0x10
00374518 jb        0x7ff7ea284523
0037451a mov       rcx, qword ptr [rbp + 0x28]
0037451e call      0x7ff7eade5628
00374523 lea       rdx, [rbp]
00374527 lea       rcx, [rip + 0x1dbeb2a]
0037452e call      0x7ff7e9f24ac4
00374533 mov       r11, rax
00374536 mov       eax, dword ptr [rbp - 0x80]
00374539 mov       dword ptr [r11 + 0x28], eax
0037453d mov       eax, dword ptr [rbp - 0x7c]
00374540 mov       dword ptr [r11 + 0x2c], eax
00374544 mov       eax, dword ptr [rbp - 0x78]
00374547 mov       dword ptr [r11 + 0x30], eax
0037454b mov       eax, dword ptr [rbp - 0x74]
0037454e mov       dword ptr [r11 + 0x34], eax
00374552 mov       eax, dword ptr [rbp - 0x70]
00374555 mov       dword ptr [r11 + 0x38], eax
00374559 inc       ebx
0037455b cmp       ebx, r12d
0037455e jl        0x7ff7ea284480
00374564 mov       dword ptr [rsp + 0x48], edi
00374568 mov       byte ptr [rsp + 0x30], 1
0037456d mov       rbx, qword ptr [rip + 0x1dbf3ec]
00374574 lea       rdx, [rbp - 0x68]
00374578 mov       rcx, qword ptr [rip + 0x1dbf0b1]
0037457f call      0x7ff7e9f29849 ; ?getCameraCenter@PlayerInterface@@QEBA?BVVector3@Ogre@@XZ
00374584 movss     xmm3, dword ptr [rax + 8]
00374589 movss     xmm2, dword ptr [rax]
0037458d lea       rdx, [rsp + 0x38]
00374592 mov       rcx, rbx
00374595 call      0x7ff7e9f46304 ; ?getMapSector@ZoneManager@@QEBA?AViVector2@@MM@Z
0037459a mov       r8d, dword ptr [rax + 4]
0037459e mov       edx, dword ptr [rax]
003745a0 mov       rcx, rbx
003745a3 call      0x7ff7e9f27bac ; ?getZoneMap@ZoneManager@@QEAAPEAVZoneMap@@HH@Z
003745a8 mov       qword ptr [rbx + 0x1681d0], rax
003745af mov       dword ptr [rbx + 0x1681d8], 1
003745b9 mov       dl, 1
003745bb lea       rcx, [rip + 0x1dbe18e]
003745c2 call      0x7ff7e9f57677 ; ?showLoadingMessage@ForgottenGUI@@QEAAX_N@Z
003745c7 mov       dl, 1
003745c9 lea       rcx, [rip + 0x1dbeae0]
003745d0 call      0x7ff7e9f615ff ; ?togglePause@GameWorld@@QEAAX_N@Z
003745d5 call      0x7ff7e9f2c841
003745da mov       rcx, rax
003745dd xor       r8d, r8d
003745e0 mov       rdx, r15
003745e3 call      0x7ff7e9f2cec2
003745e8 lea       rcx, [rip + 0x1dbeac1]
003745ef call      0x7ff7e9f640d4 ; ?loadAllPlatoons@GameWorld@@QEAAXXZ
003745f4 test      byte ptr [r13 + 0xa4], 1
003745fc je        0x7ff7ea284620
003745fe mov       rcx, qword ptr [rip + 0x1dbf02b]
00374605 mov       rbx, qword ptr [rcx + 0x2a0]
0037460c lea       rdx, [rbp - 0x68]
00374610 call      0x7ff7e9f29849 ; ?getCameraCenter@PlayerInterface@@QEBA?BVVector3@Ogre@@XZ
00374615 mov       rdx, rax
00374618 mov       rcx, rbx
0037461b call      0x7ff7e9f20fcd ; ?resetSquadPositions@Faction@@QEAA_NAEBVVector3@Ogre@@@Z
00374620 test      r15, r15
00374623 je        0x7ff7ea284714
00374629 mov       qword ptr [rbp - 0x38], 0xf
00374631 mov       qword ptr [rbp - 0x40], rdi
00374635 mov       byte ptr [rbp - 0x50], 0
00374639 mov       r8d, 0xc
0037463f lea       rdx, [rip + 0x134f3ea]
00374646 lea       rcx, [rbp - 0x50]
0037464a call      0x7ff7e9f5d7c5
0037464f nop
00374650 lea       rdx, [rbp - 0x50]
00374654 lea       rcx, [r15 + 0x178]
0037465b call      0x7ff7e9f580fe
00374660 mov       esi, dword ptr [rax + 0x28]
00374663 cmp       qword ptr [rbp - 0x38], 0x10
00374668 jb        0x7ff7ea284673
0037466a mov       rcx, qword ptr [rbp - 0x50]
0037466e call      0x7ff7eade5628
00374673 mov       rax, qword ptr [rip + 0x1dbefb6]
0037467a mov       rcx, qword ptr [rax + 0x2a0]
00374681 mov       rax, qword ptr [rcx + 0x80]
00374688 mov       dword ptr [rax + 0x88], esi
0037468e mov       rdx, r15
00374691 lea       rcx, [rip + 0x1dbde90]
00374698 call      0x7ff7e9f466a1 ; ?load@GameplayOptions@@QEAAXPEAVGameData@@@Z
0037469d mov       rcx, r15
003746a0 call      0x7ff7e9f29560 ; ?load@DialogDataManager@@SAXPEAVGameData@@@Z
003746a5 mov       rcx, qword ptr [rip + 0x1dbef84]
003746ac add       rcx, 0x110
003746b3 mov       rdx, r15
003746b6 call      0x7ff7e9f2418c ; ?load@AIOptions@PlayerInterface@@QEAAXPEAVGameData@@@Z
003746bb mov       qword ptr [rbp - 0x38], 0xf
003746c3 mov       qword ptr [rbp - 0x40], rdi
003746c7 mov       byte ptr [rbp - 0x50], 0
003746cb mov       r8d, 9
003746d1 lea       rdx, [rip + 0x134f348]
003746d8 lea       rcx, [rbp - 0x50]
003746dc call      0x7ff7e9f5d7c5
003746e1 nop
003746e2 lea       rdx, [rbp - 0x50]
003746e6 lea       rcx, [r15 + 0x178]
003746ed call      0x7ff7e9f580fe
003746f2 mov       edx, dword ptr [rax + 0x28]
003746f5 mov       rcx, qword ptr [rip + 0x1dbef34]
003746fc call      0x7ff7e9f3140e ; ?setCurrentFloor@PlayerInterface@@QEAAXH@Z
00374701 nop
00374702 cmp       qword ptr [rbp - 0x38], 0x10
00374707 jb        0x7ff7ea284719
00374709 mov       rcx, qword ptr [rbp - 0x50]
0037470d call      0x7ff7eade5628
00374712 jmp       0x7ff7ea284719
00374714 mov       byte ptr [rsp + 0x30], 0
00374719 mov       dword ptr [rsp + 0x48], edi
0037471d mov       r8d, 0x15
00374723 lea       rdx, [rsp + 0x40]
00374728 lea       rcx, [rip + 0x1dbeca1]
0037472f call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00374734 mov       rcx, qword ptr [rip + 0x1dbeef5]
0037473b mov       rcx, qword ptr [rcx + 0x38]
0037473f cmp       dword ptr [rsp + 0x48], 0
00374744 jbe       0x7ff7ea284755
00374746 mov       rax, qword ptr [rsp + 0x50]
0037474b mov       rdx, qword ptr [rax]
0037474e call      0x7ff7e9f316e8 ; ?load@Research@@QEAAXPEAVGameData@@@Z
00374753 jmp       0x7ff7ea28475a
00374755 call      0x7ff7e9f3e555 ; ?_updateTheAllList@Research@@QEAAXXZ
0037475a test      r15, r15
0037475d je        0x7ff7ea2849e3
00374763 mov       qword ptr [rbp + 0x18], 0xf
0037476b mov       qword ptr [rbp + 0x10], rdi
0037476f mov       byte ptr [rbp], 0
00374773 mov       r8d, 5
00374779 lea       rdx, [rip + 0x134f298]
00374780 lea       rcx, [rbp]
00374784 call      0x7ff7e9f5d7c5
00374789 nop
0037478a movzx     eax, byte ptr [r15 + 0x2b8]
00374792 lea       rcx, [rax + r15 + 0x2b9]
0037479a test      rcx, rcx
0037479d lea       rbx, [rcx + 1]
003747a1 jne       0x7ff7ea2847a6
003747a3 mov       rbx, rdi
003747a6 lea       rcx, [rbp]
003747aa mov       r8, qword ptr [rbp]
003747ae mov       rdx, qword ptr [rbp + 0x18]
003747b2 cmp       rdx, 0x10
003747b6 cmovae    rcx, r8
003747ba add       rcx, qword ptr [rbp + 0x10]
003747be lea       rax, [rbp]
003747c2 cmp       rdx, 0x10
003747c6 cmovae    rax, r8
003747ca mov       r10, rdi
003747cd cmp       rax, rcx
003747d0 je        0x7ff7ea284807
003747d2 mov       esi, 0x9e3779b9
003747d7 nop       word ptr [rax + rax]
003747e0 movsx     r8, byte ptr [rax]
003747e4 mov       rdx, r10
003747e7 shl       rdx, 6
003747eb mov       r9, r10
003747ee shr       r9, 2
003747f2 add       r8, rdx
003747f5 lea       rdx, [r9 + rsi]
003747f9 add       r8, rdx
003747fc xor       r10, r8
003747ff inc       rax
00374802 cmp       rax, rcx
00374805 jne       0x7ff7ea2847e0
00374807 mov       rax, r10
0037480a shl       rax, 0x15
0037480e not       r10
00374811 add       rax, r10
00374814 mov       rcx, rax
00374817 shr       rcx, 0x18
0037481b xor       rcx, rax
0037481e imul      rcx, rcx, 0x109
00374825 mov       rax, rcx
00374828 shr       rax, 0xe
0037482c xor       rax, rcx
0037482f imul      rax, rax, 0x15
00374833 mov       r8, rax
00374836 shr       r8, 0x1c
0037483a xor       r8, rax
0037483d mov       eax, 0x80000001
00374842 imul      r8, rax
00374846 mov       qword ptr [rsp + 0x20], rbx
0037484b lea       r9, [rbp]
0037484f lea       rdx, [rsp + 0x38]
00374854 lea       rcx, [r15 + 0x2b8]
0037485b call      0x7ff7e9f36b70
00374860 mov       r11, qword ptr [rsp + 0x38]
00374865 test      r11, r11
00374868 je        0x7ff7ea284877
0037486a lea       rsi, [r11 + 0x38]
0037486e mov       rax, qword ptr [rsi + 8]
00374872 cmp       qword ptr [rsi], rax
00374875 jne       0x7ff7ea28487a
00374877 mov       rsi, rdi
0037487a cmp       qword ptr [rbp + 0x18], 0x10
0037487f jb        0x7ff7ea28488a
00374881 mov       rcx, qword ptr [rbp]
00374885 call      0x7ff7eade5628
0037488a mov       qword ptr [rbp + 0x18], 0xf
00374892 mov       qword ptr [rbp + 0x10], rdi
00374896 mov       byte ptr [rbp], 0
0037489a test      rsi, rsi
0037489d je        0x7ff7ea2849e3
003748a3 mov       qword ptr [rbp + 0x40], 0xf
003748ab mov       qword ptr [rbp + 0x38], rdi
003748af mov       byte ptr [rbp + 0x28], 0
003748b3 mov       r14d, edi
003748b6 mov       r13, rdi
003748b9 mov       rdx, qword ptr [rsi]
003748bc mov       rcx, qword ptr [rsi + 8]
003748c0 sub       rcx, rdx
003748c3 sar       rcx, 6
003748c7 test      rcx, rcx
003748ca je        0x7ff7ea284926
003748cc mov       r12, rdi
003748cf nop
003748d0 cmp       rcx, r13
003748d3 jbe       0x7ff7ea284918
003748d5 cmp       dword ptr [r12 + rdx + 8], 0
003748db setne     bl
003748de mov       r8d, dword ptr [r12 + rdx + 4]
003748e3 mov       edx, dword ptr [r12 + rdx]
003748e7 mov       rcx, qword ptr [rip + 0x1dbf072]
003748ee call      0x7ff7e9f27bac ; ?getZoneMap@ZoneManager@@QEAAPEAVZoneMap@@HH@Z
003748f3 mov       byte ptr [rax + 0x24], bl
003748f6 inc       r14d
003748f9 inc       r13
003748fc add       r12, 0x40
00374900 mov       rdx, qword ptr [rsi]
00374903 mov       rcx, qword ptr [rsi + 8]
00374907 sub       rcx, rdx
0037490a sar       rcx, 6
0037490e movsxd    rax, r14d
00374911 cmp       rax, rcx
00374914 jb        0x7ff7ea2848d0
00374916 jmp       0x7ff7ea284926
00374918 lea       rcx, [rip + 0x1309f99]
0037491f call      qword ptr [rip + 0x1ecfcc3]
00374925 int3
