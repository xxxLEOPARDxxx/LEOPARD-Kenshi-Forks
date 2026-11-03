?execute@SaveManager@@QEAAXXZ RVA=47baa0 slot=6204 PID=9388
0047baa0 mov       rax, rsp
0047baa3 push      rbp
0047baa4 push      rdi
0047baa5 push      r12
0047baa7 lea       rbp, [rax - 0x598]
0047baae sub       rsp, 0x680
0047bab5 mov       qword ptr [rsp + 0x30], 0xfffffffffffffffe
0047babe mov       qword ptr [rax + 0x10], rbx
0047bac2 mov       qword ptr [rax + 0x18], rsi
0047bac6 mov       rax, qword ptr [rip + 0x1c9a8eb]
0047bacd xor       rax, rsp
0047bad0 mov       qword ptr [rbp + 0x570], rax
0047bad7 mov       rdi, rcx
0047bada mov       eax, dword ptr [rcx + 0xa8]
0047bae0 test      eax, eax
0047bae2 jle       0x7ff7ea38baf1
0047bae4 dec       eax
0047bae6 mov       dword ptr [rcx + 0xa8], eax
0047baec jmp       0x7ff7ea38c2ec
0047baf1 mov       eax, dword ptr [rip + 0x1cb211d]
0047baf7 xor       r12d, r12d
0047bafa test      al, 1
0047bafc jne       0x7ff7ea38bb31
0047bafe or        eax, 1
0047bb01 mov       dword ptr [rip + 0x1cb210d], eax
0047bb07 mov       ecx, 0x318
0047bb0c call      0x7ff7eade563a
0047bb11 mov       qword ptr [rsp + 0x38], rax
0047bb16 test      rax, rax
0047bb19 je        0x7ff7ea38bb25
0047bb1b mov       rcx, rax
0047bb1e call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
0047bb23 jmp       0x7ff7ea38bb28
0047bb25 mov       rax, r12
0047bb28 mov       qword ptr [rip + 0x1cb20d9], rax
0047bb2f jmp       0x7ff7ea38bb38
0047bb31 mov       rax, qword ptr [rip + 0x1cb20d0]
0047bb38 cmp       dword ptr [rax + 0x1e0], 0
0047bb3f jg        0x7ff7ea38c2ec
0047bb45 mov       ecx, dword ptr [rdi + 0xa0]
0047bb4b test      ecx, ecx
0047bb4d je        0x7ff7ea38c2ec
0047bb53 mov       dword ptr [rdi + 0x118], r12d
0047bb5a mov       byte ptr [rdi + 0x11c], 1
0047bb61 dec       ecx
0047bb63 je        0x7ff7ea38c10a
0047bb69 dec       ecx
0047bb6b je        0x7ff7ea38bf5c
0047bb71 dec       ecx
0047bb73 je        0x7ff7ea38bcd8
0047bb79 dec       ecx
0047bb7b jne       0x7ff7ea38c1f1
0047bb81 lea       rcx, [rdi + 0x28]
0047bb85 mov       r8d, 6
0047bb8b lea       rdx, [rip + 0x1247c7a]
0047bb92 call      0x7ff7e9f5d7c5
0047bb97 xor       edx, edx
0047bb99 lea       rcx, [rip + 0x1cb6710]
0047bba0 call      0x7ff7e9f44b67 ; ?newGame@KingOfRenderThread@@QEAAXPEAVWidget@MyGUI@@@Z
0047bba5 lea       rcx, [rip + 0x1cb6704]
0047bbac call      0x7ff7e9f42ff6 ; ?deleteTitleScreen@KingOfRenderThread@@QEAAXXZ
0047bbb1 lea       rcx, [rip + 0x1cb74f8]
0047bbb8 call      0x7ff7e9f51c31 ; ?resetGame@GameWorld@@QEAAXXZ
0047bbbd lea       rdx, [rip + 0x1cb768c]
0047bbc4 lea       rcx, [rip + 0x1cb74e5]
0047bbcb call      0x7ff7e9f38c68 ; ?initialiseNewGameWorld@GameWorld@@QEAAXPEAVGameDataManager@@@Z
0047bbd0 mov       dl, 1
0047bbd2 mov       rcx, qword ptr [rip + 0x1cb74c7]
0047bbd9 call      0x7ff7e9f1f52e ; ?loadLevelData@TownList@@QEAAX_N@Z
0047bbde mov       rcx, qword ptr [rip + 0x1cb7a4b]
0047bbe5 mov       rcx, qword ptr [rcx + 0x38]
0047bbe9 call      0x7ff7e9f3e555 ; ?_updateTheAllList@Research@@QEAAXXZ
0047bbee mov       rcx, qword ptr [rip + 0x1cb7d7b]
0047bbf5 mov       rax, qword ptr [rcx]
0047bbf8 call      qword ptr [rax + 0x20]
0047bbfb call      0x7ff7e9f5ecb5 ; ?getInstance@PortraitManager@@SAPEAV1@XZ
0047bc00 mov       rcx, rax
0047bc03 call      0x7ff7e9f302bb ; ?init@PortraitManager@@QEAAXXZ
0047bc08 call      0x7ff7e9f5eea4
0047bc0d lea       rdx, [rdi + 0xb0]
0047bc14 mov       r8d, 0x40
0047bc1a lea       rcx, [rip + 0x1cb74af]
0047bc21 call      0x7ff7e9f51aa6 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4itemType@@@Z
0047bc26 mov       rbx, rax
0047bc29 lea       r8, [rax + 0x28]
0047bc2d lea       rdx, [rip + 0x125c89c]
0047bc34 lea       rcx, [rbp + 0x188]
0047bc3b call      0x7ff7e9f3851f
0047bc40 nop
0047bc41 lea       r8, [rip + 0x1206f40]
0047bc48 mov       rdx, rax
0047bc4b lea       rcx, [rsp + 0x40]
0047bc50 call      0x7ff7e9f42d6c
0047bc55 nop
0047bc56 mov       edx, 2
0047bc5b mov       rcx, rax
0047bc5e call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0047bc63 nop
0047bc64 cmp       qword ptr [rsp + 0x58], 0x10
0047bc6a jb        0x7ff7ea38bc76
0047bc6c mov       rcx, qword ptr [rsp + 0x40]
0047bc71 call      0x7ff7eade5628
0047bc76 mov       qword ptr [rsp + 0x58], 0xf
0047bc7f mov       qword ptr [rsp + 0x50], r12
0047bc84 mov       byte ptr [rsp + 0x40], 0
0047bc89 cmp       qword ptr [rbp + 0x1a0], 0x10
0047bc91 jb        0x7ff7ea38bc9f
0047bc93 mov       rcx, qword ptr [rbp + 0x188]
0047bc9a call      0x7ff7eade5628
0047bc9f mov       qword ptr [rbp + 0x1a0], 0xf
0047bcaa mov       qword ptr [rbp + 0x198], r12
0047bcb1 mov       byte ptr [rbp + 0x188], 0
0047bcb8 mov       rdx, rbx
0047bcbb lea       rcx, [rip + 0x1cb65ee]
0047bcc2 call      0x7ff7e9f5377a ; ?newGameWithCharEdit@KingOfRenderThread@@QEAAXPEAVGameData@@@Z
0047bcc7 lea       rcx, [rip + 0x1cb65e2]
0047bcce call      0x7ff7e9f42ff6 ; ?deleteTitleScreen@KingOfRenderThread@@QEAAXXZ
0047bcd3 jmp       0x7ff7ea38c1f1
0047bcd8 cmp       qword ptr [rip + 0x1cb65f0], 0
0047bce0 je        0x7ff7ea38bcf0
0047bce2 xor       edx, edx
0047bce4 lea       rcx, [rip + 0x1cb65c5]
0047bceb call      0x7ff7e9f44b67 ; ?newGame@KingOfRenderThread@@QEAAXPEAVWidget@MyGUI@@@Z
0047bcf0 lea       rcx, [rip + 0x1cb65b9]
0047bcf7 call      0x7ff7e9f42ff6 ; ?deleteTitleScreen@KingOfRenderThread@@QEAAXXZ
0047bcfc xor       r8d, r8d
0047bcff mov       dword ptr [rsp + 0x20], r12d
0047bd04 mov       r9b, 0x20
0047bd07 mov       edx, dword ptr [rdi + 0xa4]
0047bd0d lea       rcx, [rbp + 0x70]
0047bd11 call      qword ptr [rip + 0x1dca3c9]
0047bd17 mov       rbx, rax
0047bd1a lea       r8, [rdi + 0xb0]
0047bd21 lea       rdx, [rip + 0x125c788]
0047bd28 lea       rcx, [rbp + 0xe8]
0047bd2f call      0x7ff7e9f3851f
0047bd34 nop
0047bd35 lea       r8, [rip + 0x125c764]
0047bd3c mov       rdx, rax
0047bd3f lea       rcx, [rbp + 0x20]
0047bd43 call      0x7ff7e9f42d6c
0047bd48 nop
0047bd49 mov       r8, rbx
0047bd4c mov       rdx, rax
0047bd4f lea       rcx, [rbp + 0x98]
0047bd56 call      0x7ff7e9f5606a
0047bd5b nop
0047bd5c lea       r8, [rip + 0x1247329]
0047bd63 mov       rdx, rax
0047bd66 lea       rcx, [rbp + 0x48]
0047bd6a call      0x7ff7e9f42d6c
0047bd6f nop
0047bd70 mov       edx, 2
0047bd75 mov       rcx, rax
0047bd78 call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0047bd7d nop
0047bd7e cmp       qword ptr [rbp + 0x60], 0x10
0047bd83 jb        0x7ff7ea38bd8e
0047bd85 mov       rcx, qword ptr [rbp + 0x48]
0047bd89 call      0x7ff7eade5628
0047bd8e mov       qword ptr [rbp + 0x60], 0xf
0047bd96 mov       qword ptr [rbp + 0x58], r12
0047bd9a mov       byte ptr [rbp + 0x48], 0
0047bd9e cmp       qword ptr [rbp + 0xb0], 0x10
0047bda6 jb        0x7ff7ea38bdb4
0047bda8 mov       rcx, qword ptr [rbp + 0x98]
0047bdaf call      0x7ff7eade5628
0047bdb4 mov       qword ptr [rbp + 0xb0], 0xf
0047bdbf mov       qword ptr [rbp + 0xa8], r12
0047bdc6 mov       byte ptr [rbp + 0x98], 0
0047bdcd cmp       qword ptr [rbp + 0x38], 0x10
0047bdd2 jb        0x7ff7ea38bddd
0047bdd4 mov       rcx, qword ptr [rbp + 0x20]
0047bdd8 call      0x7ff7eade5628
0047bddd mov       qword ptr [rbp + 0x38], 0xf
0047bde5 mov       qword ptr [rbp + 0x30], r12
0047bde9 mov       byte ptr [rbp + 0x20], 0
0047bded cmp       qword ptr [rbp + 0x100], 0x10
0047bdf5 jb        0x7ff7ea38be03
0047bdf7 mov       rcx, qword ptr [rbp + 0xe8]
0047bdfe call      0x7ff7eade5628
0047be03 mov       qword ptr [rbp + 0x100], 0xf
0047be0e mov       qword ptr [rbp + 0xf8], r12
0047be15 mov       byte ptr [rbp + 0xe8], 0
0047be1c cmp       qword ptr [rbp + 0x88], 0x10
0047be24 jb        0x7ff7ea38be2f
0047be26 mov       rcx, qword ptr [rbp + 0x70]
0047be2a call      0x7ff7eade5628
0047be2f mov       qword ptr [rbp + 0x88], 0xf
0047be3a mov       qword ptr [rbp + 0x80], r12
0047be41 mov       byte ptr [rbp + 0x70], 0
0047be45 lea       rdx, [rdi + 0xd8]
0047be4c mov       r9d, dword ptr [rdi + 0xa4]
0047be53 lea       r8, [rdi + 0xb0]
0047be5a mov       rcx, rdi
0047be5d call      0x7ff7e9f38c40 ; ?importGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H@Z
0047be62 cmp       eax, 2
0047be65 je        0x7ff7ea38c2ec
0047be6b test      eax, eax
0047be6d je        0x7ff7ea38c1f1
0047be73 lea       rdx, [rip + 0x125c606]
0047be7a lea       rcx, [rbp + 0x2f0]
0047be81 call      0x7ff7e9f210b8
0047be86 nop
0047be87 lea       rdx, [rbp + 0xc0]
0047be8e mov       rcx, rax
0047be91 call      0x7ff7e9f18a3a
0047be96 mov       rbx, rax
0047be99 lea       rdx, [rip + 0x122d108]
0047bea0 lea       rcx, [rbp + 0x1b0]
0047bea7 call      0x7ff7e9f210b8
0047beac nop
0047bead lea       rdx, [rbp + 0x138]
0047beb4 mov       rcx, rax
0047beb7 call      0x7ff7e9f18a3a
0047bebc nop
0047bebd mov       qword ptr [rsp + 0x28], r12
0047bec2 mov       byte ptr [rsp + 0x20], 1
0047bec7 mov       r9d, 1
0047becd mov       r8, rbx
0047bed0 mov       rdx, rax
0047bed3 lea       rcx, [rip + 0x1cb6876]
0047beda call      0x7ff7e9f34032 ; ?messageBox@ForgottenGUI@@QEAAPEAVWindow@MyGUI@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H_NPEAV?$IDelegate1@H@delegates@3@@Z
0047bedf nop
0047bee0 cmp       qword ptr [rbp + 0x150], 0x10
0047bee8 jb        0x7ff7ea38bef6
0047beea mov       rcx, qword ptr [rbp + 0x138]
0047bef1 call      0x7ff7eade5628
0047bef6 mov       qword ptr [rbp + 0x150], 0xf
0047bf01 mov       qword ptr [rbp + 0x148], r12
0047bf08 mov       byte ptr [rbp + 0x138], 0
0047bf0f lea       rcx, [rbp + 0x1b0]
0047bf16 call      0x7ff7e9f13c9c
0047bf1b nop
0047bf1c cmp       qword ptr [rbp + 0xd8], 0x10
0047bf24 jb        0x7ff7ea38bf32
0047bf26 mov       rcx, qword ptr [rbp + 0xc0]
0047bf2d call      0x7ff7eade5628
0047bf32 mov       qword ptr [rbp + 0xd8], 0xf
0047bf3d mov       qword ptr [rbp + 0xd0], r12
0047bf44 mov       byte ptr [rbp + 0xc0], 0
0047bf4b lea       rcx, [rbp + 0x2f0]
0047bf52 call      0x7ff7e9f13c9c
0047bf57 jmp       0x7ff7ea38c2b9
0047bf5c cmp       qword ptr [rip + 0x1cb636c], 0
0047bf64 je        0x7ff7ea38bf74
0047bf66 xor       edx, edx
0047bf68 lea       rcx, [rip + 0x1cb6341]
0047bf6f call      0x7ff7e9f44b67 ; ?newGame@KingOfRenderThread@@QEAAXPEAVWidget@MyGUI@@@Z
0047bf74 lea       rcx, [rip + 0x1cb6335]
0047bf7b call      0x7ff7e9f42ff6 ; ?deleteTitleScreen@KingOfRenderThread@@QEAAXXZ
0047bf80 lea       r8, [rdi + 0xb0]
0047bf87 lea       rdx, [rip + 0x125c4d2]
0047bf8e lea       rcx, [rbp + 0x110]
0047bf95 call      0x7ff7e9f3851f
0047bf9a nop
0047bf9b lea       r8, [rip + 0x1206be6]
0047bfa2 mov       rdx, rax
0047bfa5 lea       rcx, [rbp - 0x30]
0047bfa9 call      0x7ff7e9f42d6c
0047bfae nop
0047bfaf mov       edx, 2
0047bfb4 mov       rcx, rax
0047bfb7 call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0047bfbc nop
0047bfbd cmp       qword ptr [rbp - 0x18], 0x10
0047bfc2 jb        0x7ff7ea38bfcd
0047bfc4 mov       rcx, qword ptr [rbp - 0x30]
0047bfc8 call      0x7ff7eade5628
0047bfcd mov       qword ptr [rbp - 0x18], 0xf
0047bfd5 mov       qword ptr [rbp - 0x20], r12
0047bfd9 mov       byte ptr [rbp - 0x30], 0
0047bfdd cmp       qword ptr [rbp + 0x128], 0x10
0047bfe5 jb        0x7ff7ea38bff3
0047bfe7 mov       rcx, qword ptr [rbp + 0x110]
0047bfee call      0x7ff7eade5628
0047bff3 mov       qword ptr [rbp + 0x128], 0xf
0047bffe mov       qword ptr [rbp + 0x120], r12
0047c005 mov       byte ptr [rbp + 0x110], 0
0047c00c lea       rdx, [rdi + 0xd8]
0047c013 lea       r8, [rdi + 0xb0]
0047c01a mov       rcx, rdi
0047c01d call      0x7ff7e9f228dc ; ?loadGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
0047c022 cmp       eax, 2
0047c025 je        0x7ff7ea38c2ec
0047c02b test      eax, eax
0047c02d je        0x7ff7ea38c1f1
0047c033 lea       rdx, [rip + 0x125c40e]
0047c03a lea       rcx, [rbp + 0x250]
0047c041 call      0x7ff7e9f210b8
0047c046 nop
0047c047 lea       rdx, [rbp - 8]
0047c04b mov       rcx, rax
0047c04e call      0x7ff7e9f18a3a
0047c053 mov       rbx, rax
0047c056 lea       rdx, [rip + 0x122cf4b]
0047c05d lea       rcx, [rbp + 0x430]
0047c064 call      0x7ff7e9f210b8
0047c069 nop
0047c06a lea       rdx, [rbp + 0x160]
0047c071 mov       rcx, rax
0047c074 call      0x7ff7e9f18a3a
0047c079 nop
0047c07a mov       qword ptr [rsp + 0x28], r12
0047c07f mov       byte ptr [rsp + 0x20], 1
0047c084 mov       r9d, 1
0047c08a mov       r8, rbx
0047c08d mov       rdx, rax
0047c090 lea       rcx, [rip + 0x1cb66b9]
0047c097 call      0x7ff7e9f34032 ; ?messageBox@ForgottenGUI@@QEAAPEAVWindow@MyGUI@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H_NPEAV?$IDelegate1@H@delegates@3@@Z
0047c09c nop
0047c09d cmp       qword ptr [rbp + 0x178], 0x10
0047c0a5 jb        0x7ff7ea38c0b3
0047c0a7 mov       rcx, qword ptr [rbp + 0x160]
0047c0ae call      0x7ff7eade5628
0047c0b3 mov       qword ptr [rbp + 0x178], 0xf
0047c0be mov       qword ptr [rbp + 0x170], r12
0047c0c5 mov       byte ptr [rbp + 0x160], 0
0047c0cc lea       rcx, [rbp + 0x430]
0047c0d3 call      0x7ff7e9f13c9c
0047c0d8 nop
0047c0d9 cmp       qword ptr [rbp + 0x10], 0x10
0047c0de jb        0x7ff7ea38c0e9
0047c0e0 mov       rcx, qword ptr [rbp - 8]
0047c0e4 call      0x7ff7eade5628
0047c0e9 mov       qword ptr [rbp + 0x10], 0xf
0047c0f1 mov       qword ptr [rbp + 8], r12
0047c0f5 mov       byte ptr [rbp - 8], 0
0047c0f9 lea       rcx, [rbp + 0x250]
0047c100 call      0x7ff7e9f13c9c
0047c105 jmp       0x7ff7ea38c2b9
0047c10a lea       r8, [rdi + 0xb0]
0047c111 lea       rdx, [rdi + 0xd8]
0047c118 mov       rcx, rdi
0047c11b call      0x7ff7e9f22df0 ; ?saveGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
0047c120 test      eax, eax
0047c122 je        0x7ff7ea38c1f1
0047c128 lea       rdx, [rip + 0x125c301]
0047c12f lea       rcx, [rbp + 0x4d0]
0047c136 call      0x7ff7e9f210b8
0047c13b nop
0047c13c lea       rdx, [rsp + 0x68]
0047c141 mov       rcx, rax
0047c144 call      0x7ff7e9f18a3a
0047c149 mov       rbx, rax
0047c14c lea       rdx, [rip + 0x122ce55]
0047c153 lea       rcx, [rbp + 0x390]
0047c15a call      0x7ff7e9f210b8
0047c15f nop
0047c160 lea       rdx, [rbp - 0x70]
0047c164 mov       rcx, rax
0047c167 call      0x7ff7e9f18a3a
0047c16c nop
0047c16d mov       qword ptr [rsp + 0x28], r12
0047c172 mov       byte ptr [rsp + 0x20], 1
0047c177 mov       r9d, 1
0047c17d mov       r8, rbx
0047c180 mov       rdx, rax
0047c183 lea       rcx, [rip + 0x1cb65c6]
0047c18a call      0x7ff7e9f34032 ; ?messageBox@ForgottenGUI@@QEAAPEAVWindow@MyGUI@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H_NPEAV?$IDelegate1@H@delegates@3@@Z
0047c18f nop
0047c190 cmp       qword ptr [rbp - 0x58], 0x10
0047c195 jb        0x7ff7ea38c1a0
0047c197 mov       rcx, qword ptr [rbp - 0x70]
0047c19b call      0x7ff7eade5628
0047c1a0 mov       qword ptr [rbp - 0x58], 0xf
0047c1a8 mov       qword ptr [rbp - 0x60], r12
0047c1ac mov       byte ptr [rbp - 0x70], 0
0047c1b0 lea       rcx, [rbp + 0x390]
0047c1b7 call      0x7ff7e9f13c9c
0047c1bc nop
0047c1bd cmp       qword ptr [rbp - 0x80], 0x10
0047c1c2 jb        0x7ff7ea38c1ce
0047c1c4 mov       rcx, qword ptr [rsp + 0x68]
0047c1c9 call      0x7ff7eade5628
0047c1ce mov       qword ptr [rbp - 0x80], 0xf
0047c1d6 mov       qword ptr [rsp + 0x78], r12
0047c1db mov       byte ptr [rsp + 0x68], 0
0047c1e0 lea       rcx, [rbp + 0x4d0]
0047c1e7 call      0x7ff7e9f13c9c
0047c1ec jmp       0x7ff7ea38c2b9
0047c1f1 cmp       dword ptr [rdi + 0xa0], 1
0047c1f8 jne       0x7ff7ea38c2b9
0047c1fe or        r9, 0xffffffffffffffff
0047c202 xor       r8d, r8d
0047c205 lea       rdx, [rdi + 0xb0]
0047c20c mov       rcx, rdi
0047c20f call      0x7ff7e9f3808d
0047c214 mov       qword ptr [rbp - 0x80], 0xf
0047c21c mov       qword ptr [rsp + 0x78], r12
0047c221 mov       byte ptr [rsp + 0x68], 0
0047c226 mov       r8d, 0xc
0047c22c lea       rdx, [rip + 0x121a665]
0047c233 lea       rcx, [rsp + 0x68]
0047c238 call      0x7ff7e9f5d7c5
0047c23d lea       rdx, [rsp + 0x68]
0047c242 lea       rcx, [rbp - 0x70]
0047c246 call      0x7ff7e9f14d40
0047c24b nop
0047c24c mov       qword ptr [rsp + 0x58], 0xf
0047c255 mov       qword ptr [rsp + 0x50], r12
0047c25a mov       byte ptr [rsp + 0x40], 0
0047c25f mov       r8d, 8
0047c265 lea       rdx, [rip + 0x1250ee4]
0047c26c lea       rcx, [rsp + 0x40]
0047c271 call      0x7ff7e9f5d7c5
0047c276 nop
0047c277 lea       r8, [rdi + 0xb0]
0047c27e lea       rdx, [rsp + 0x40]
0047c283 lea       rcx, [rbp - 0x70]
0047c287 call      0x7ff7e9f57e60
0047c28c nop
0047c28d cmp       qword ptr [rsp + 0x58], 0x10
0047c293 jb        0x7ff7ea38c29f
0047c295 mov       rcx, qword ptr [rsp + 0x40]
0047c29a call      0x7ff7eade5628
0047c29f lea       rdx, [rip + 0x1afbdf2]
0047c2a6 lea       rcx, [rbp - 0x70]
0047c2aa call      0x7ff7e9f5fd40
0047c2af nop
0047c2b0 lea       rcx, [rbp - 0x70]
0047c2b4 call      0x7ff7e9f2238c
0047c2b9 mov       eax, dword ptr [rdi + 0xa0]
0047c2bf test      eax, eax
0047c2c1 je        0x7ff7ea38c2e5
0047c2c3 cmp       eax, 1
0047c2c6 jne       0x7ff7ea38c2d0
0047c2c8 cmp       dword ptr [rdi + 0xa4], eax
0047c2ce je        0x7ff7ea38c2e5
0047c2d0 mov       rcx, rdi
0047c2d3 call      0x7ff7e9f4cd2b ; ?hide@SaveManager@@QEAAXXZ
0047c2d8 call      0x7ff7e9f5fad9
0047c2dd mov       rcx, rax
0047c2e0 call      0x7ff7e9f48b9f
0047c2e5 mov       dword ptr [rdi + 0xa0], r12d
0047c2ec mov       rcx, qword ptr [rbp + 0x570]
0047c2f3 xor       rcx, rsp
0047c2f6 call      0x7ff7eade5db0
0047c2fb lea       r11, [rsp + 0x680]
0047c303 mov       rbx, qword ptr [r11 + 0x28]
0047c307 mov       rsi, qword ptr [r11 + 0x30]
0047c30b mov       rsp, r11
0047c30e pop       r12
0047c310 pop       rdi
0047c311 pop       rbp
0047c312 ret
0047c313 int3
