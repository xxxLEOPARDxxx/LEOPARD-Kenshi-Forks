?importGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H@Z RVA=3788f0 slot=6214 PID=9388
003788f0 mov       rax, rsp
003788f3 push      rbp
003788f4 push      rsi
003788f5 push      rdi
003788f6 push      r12
003788f8 push      r13
003788fa push      r14
003788fc push      r15
003788fe lea       rbp, [rax - 0x388]
00378905 sub       rsp, 0x450
0037890c mov       qword ptr [rbp - 0x40], 0xfffffffffffffffe
00378914 mov       qword ptr [rax + 0x20], rbx
00378918 movaps    xmmword ptr [rax - 0x48], xmm6
0037891c movaps    xmmword ptr [rax - 0x58], xmm7
00378920 movaps    xmmword ptr [rax - 0x68], xmm8
00378925 mov       rax, qword ptr [rip + 0x1d9da8c]
0037892c xor       rax, rsp
0037892f mov       qword ptr [rbp + 0x310], rax
00378936 mov       ebx, r9d
00378939 mov       dword ptr [rsp + 0x34], ebx
0037893d mov       r15, r8
00378940 mov       qword ptr [rbp - 0x58], r8
00378944 mov       rsi, rdx
00378947 mov       qword ptr [rbp - 0x60], rdx
0037894b mov       rdi, rcx
0037894e xor       r13d, r13d
00378951 mov       r14d, r13d
00378954 mov       dword ptr [rsp + 0x30], r13d
00378959 mov       dword ptr [rsp + 0x50], r13d
0037895e call      0x7ff7e9f561af ; ?saveExists@SaveManager@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
00378963 test      al, al
00378965 jne       0x7ff7ea288970
00378967 lea       eax, [r13 + 1]
0037896b jmp       0x7ff7ea289e1a
00378970 mov       r8, r15
00378973 lea       rdx, [rip + 0x134b4ee]
0037897a lea       rcx, [rbp + 0x90]
00378981 call      0x7ff7e9f3851f
00378986 nop
00378987 lea       r8, [rip + 0x130a40a]
0037898e mov       rdx, rax
00378991 lea       rcx, [rbp - 0x38]
00378995 call      0x7ff7e9f42d6c
0037899a nop
0037899b mov       edx, 2
003789a0 mov       rcx, rax
003789a3 call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
003789a8 nop
003789a9 cmp       qword ptr [rbp - 0x20], 0x10
003789ae jb        0x7ff7ea2889b9
003789b0 mov       rcx, qword ptr [rbp - 0x38]
003789b4 call      0x7ff7eade5628
003789b9 mov       qword ptr [rbp - 0x20], 0xf
003789c1 mov       qword ptr [rbp - 0x28], r13
003789c5 mov       byte ptr [rbp - 0x38], 0
003789c9 cmp       qword ptr [rbp + 0xa8], 0x10
003789d1 jb        0x7ff7ea2889df
003789d3 mov       rcx, qword ptr [rbp + 0x90]
003789da call      0x7ff7eade5628
003789df lea       rcx, [rdi + 0x28]
003789e3 mov       r8d, 6
003789e9 lea       rdx, [rip + 0x134ae1c]
003789f0 call      0x7ff7e9f5d7c5
003789f5 lea       rcx, [rip + 0x1dba6b4]
003789fc call      0x7ff7e9f51c31 ; ?resetGame@GameWorld@@QEAAXXZ
00378a01 lea       rcx, [rip + 0x1dba9c8]
00378a08 call      0x7ff7e9f24556 ; ?clearAndDestroy@GameDataContainer@@QEAAXXZ
00378a0d lea       rdx, [rip + 0x1dba9bc]
00378a14 lea       rcx, [rip + 0x1dba695]
00378a1b call      0x7ff7e9f38c68 ; ?initialiseNewGameWorld@GameWorld@@QEAAXPEAVGameDataManager@@@Z
00378a20 call      0x7ff7e9f27cba ; ?newGameReset@DialogDataManager@@SAXXZ
00378a25 mov       rcx, qword ptr [rip + 0x1dba69c]
00378a2c call      0x7ff7e9f3505e ; ?queuesAreClearMT@PhysicsInterface@@QEAA_NXZ
00378a31 test      al, al
00378a33 jne       0x7ff7ea288a3f
00378a35 mov       eax, 2
00378a3a jmp       0x7ff7ea289e1a
00378a3f mov       r8, r15
00378a42 mov       rdx, rsi
00378a45 lea       rcx, [rbp + 0x90]
00378a4c call      0x7ff7e9f36945
00378a51 nop
00378a52 lea       r8, [rip + 0x134b3ff]
00378a59 mov       rdx, rax
00378a5c lea       rcx, [rbp + 0x18]
00378a60 call      0x7ff7e9f42d6c
00378a65 mov       qword ptr [rsp + 0x20], r13
00378a6a xor       r9d, r9d
00378a6d mov       r8b, 1
00378a70 mov       rdx, rax
00378a73 lea       rcx, [rip + 0x1dba956]
00378a7a call      0x7ff7e9f47a8d ; ?loadGameDataReturn@GameDataContainer@@QEAA_NV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N1PEAVSerialisable@@@Z
00378a7f nop
00378a80 cmp       qword ptr [rbp + 0xa8], 0x10
00378a88 jb        0x7ff7ea288a96
00378a8a mov       rcx, qword ptr [rbp + 0x90]
00378a91 call      0x7ff7eade5628
00378a96 mov       rax, qword ptr [rip + 0x1dbab93]
00378a9d mov       rcx, qword ptr [rax + 0x2a0]
00378aa4 call      0x7ff7e9f1418d ; ?clearAndDestroy@Faction@@QEAAXXZ
00378aa9 mov       rcx, qword ptr [rip + 0x1dbaab8]
00378ab0 call      0x7ff7e9f23340
00378ab5 lea       rcx, [rip + 0x1dba5f4]
00378abc call      0x7ff7e9f52a05 ; ?flushKillList@GameWorld@@QEAAXXZ
00378ac1 mov       rcx, qword ptr [rip + 0x1dbaa90]
00378ac8 call      0x7ff7e9f15b69 ; ?setupAndLinkAllFactions@FactionManager@@QEAAXXZ
00378acd movss     xmm1, dword ptr [rip + 0x1305fff]
00378ad5 mov       rcx, qword ptr [rip + 0x1db68e4]
00378adc call      0x7ff7e9f30acc
00378ae1 call      0x7ff7e9f5ecb5 ; ?getInstance@PortraitManager@@SAPEAV1@XZ
00378ae6 mov       rcx, rax
00378ae9 call      0x7ff7e9f446e9 ; ?reloadTexture@PortraitManager@@QEAAXXZ
00378aee mov       dl, 1
00378af0 mov       rcx, qword ptr [rip + 0x1dba5a9]
00378af7 call      0x7ff7e9f1f52e ; ?loadLevelData@TownList@@QEAAX_N@Z
00378afc mov       esi, ebx
00378afe and       esi, 4
00378b01 je        0x7ff7ea288b44
00378b03 lea       rdx, [rip + 0x1dba8c6]
00378b0a mov       rcx, qword ptr [rip + 0x1dba58f]
00378b11 call      0x7ff7e9f60e93 ; ?loadAllTowns_worldInit@TownList@@QEAAXPEAVGameDataContainer@@@Z
00378b16 mov       r8b, 1
00378b19 lea       rdx, [rip + 0x1dba8b0]
00378b20 mov       rcx, qword ptr [rip + 0x1dba579]
00378b27 call      0x7ff7e9f46c91 ; ?loadState@TownList@@QEAAXPEAVGameDataContainer@@_N@Z
00378b2c mov       rcx, qword ptr [rip + 0x1dba56d]
00378b33 call      0x7ff7e9f3c381 ; ?listUniqueTownsFromPlayerOwnership@TownList@@QEAAXXZ
00378b38 mov       rcx, qword ptr [rip + 0x1dba561]
00378b3f call      0x7ff7e9f34a32 ; ?destroyAllTheNests@TownList@@QEAAXXZ
00378b44 mov       r8d, ebx
00378b47 shr       r8d, 4
00378b4b and       r8b, 1
00378b4f lea       rdx, [rip + 0x1dba87a]
00378b56 mov       rcx, qword ptr [rip + 0x1dba543]
00378b5d call      0x7ff7e9f58536 ; ?loadStateDiscovery@TownList@@QEAAXPEAVGameDataContainer@@_N@Z
00378b62 mov       r11, qword ptr [rip + 0x1dba537]
00378b69 cmp       dword ptr [r11 + 0x58], 0
00378b6e jne       0x7ff7ea288bb6
00378b70 mov       qword ptr [rbp - 0x20], 0xf
00378b78 mov       qword ptr [rbp - 0x28], r13
00378b7c mov       byte ptr [rbp - 0x38], 0
00378b80 mov       r8d, 0x2a
00378b86 lea       rdx, [rip + 0x134b293]
00378b8d lea       rcx, [rbp - 0x38]
00378b91 call      0x7ff7e9f5d7c5
00378b96 nop
00378b97 mov       edx, 3
00378b9c lea       rcx, [rbp - 0x38]
00378ba0 call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
00378ba5 nop
00378ba6 cmp       qword ptr [rbp - 0x20], 0x10
00378bab jb        0x7ff7ea288bb6
00378bad mov       rcx, qword ptr [rbp - 0x38]
00378bb1 call      0x7ff7eade5628
00378bb6 call      0x7ff7e9f5eea4
00378bbb nop
00378bbc lea       rax, [rip + 0x130591d]
00378bc3 mov       qword ptr [rsp + 0x70], rax
00378bc8 mov       dword ptr [rsp + 0x78], r13d
00378bcd mov       ecx, 0xa
00378bd2 mov       dword ptr [rsp + 0x7c], ecx
00378bd6 mov       qword ptr [rbp - 0x80], r13
00378bda mov       eax, 8
00378bdf mul       rcx
00378be2 mov       rcx, 0xffffffffffffffff
00378be9 cmovo     rax, rcx
00378bed mov       rcx, rax
00378bf0 call      0x7ff7eade5634
00378bf5 mov       qword ptr [rbp - 0x80], rax
00378bf9 mov       r8d, 0x38
00378bff lea       rdx, [rsp + 0x70]
00378c04 lea       rcx, [rip + 0x1dba7c5]
00378c0b call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00378c10 cmp       dword ptr [rsp + 0x78], 0
00378c15 jbe       0x7ff7ea288c38
00378c17 mov       rax, qword ptr [rbp - 0x80]
00378c1b mov       r12, qword ptr [rax]
00378c1e mov       qword ptr [rbp - 0x70], r12
00378c22 test      r12, r12
00378c25 je        0x7ff7ea288c3f
00378c27 mov       rdx, r12
00378c2a mov       rcx, qword ptr [rip + 0x1db678f]
00378c31 call      0x7ff7e9f1352b
00378c36 jmp       0x7ff7ea288c3f
00378c38 mov       r12, r13
00378c3b mov       qword ptr [rbp - 0x70], r13
00378c3f mov       qword ptr [rbp - 0x20], 0xf
00378c47 mov       qword ptr [rbp - 0x28], r13
00378c4b mov       byte ptr [rbp - 0x38], 0
00378c4f mov       r8d, 7
00378c55 lea       rdx, [rip + 0x133098c]
00378c5c lea       rcx, [rbp - 0x38]
00378c60 call      0x7ff7e9f5d7c5
00378c65 nop
00378c66 lea       rdx, [rbp - 0x38]
00378c6a lea       rcx, [r12 + 0x138]
00378c72 call      0x7ff7e9f11aeb
00378c77 lea       rdx, [rax + 0x28]
00378c7b mov       rcx, rdi
00378c7e call      0x7ff7e9f30fc7 ; ?versionCode@SaveManager@@QEAAIAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00378c83 mov       ebx, eax
00378c85 cmp       qword ptr [rbp - 0x20], 0x10
00378c8a jb        0x7ff7ea288c95
00378c8c mov       rcx, qword ptr [rbp - 0x38]
00378c90 call      0x7ff7eade5628
00378c95 test      ebx, ebx
00378c97 jne       0x7ff7ea288ce1
00378c99 mov       qword ptr [rbp - 0x20], 0xf
00378ca1 mov       qword ptr [rbp - 0x28], r13
00378ca5 mov       byte ptr [rbp - 0x38], bl
00378ca8 lea       r8d, [rbx + 0xf]
00378cac lea       rdx, [rip + 0x134b155]
00378cb3 lea       rcx, [rbp - 0x38]
00378cb7 call      0x7ff7e9f5d7c5
00378cbc nop
00378cbd lea       rcx, [r12 + 0x178]
00378cc5 lea       rdx, [rbp - 0x38]
00378cc9 call      0x7ff7e9f580fe
00378cce mov       ebx, dword ptr [rax + 0x28]
00378cd1 cmp       qword ptr [rbp - 0x20], 0x10
00378cd6 jb        0x7ff7ea288ce1
00378cd8 mov       rcx, qword ptr [rbp - 0x38]
00378cdc call      0x7ff7eade5628
00378ce1 test      esi, esi
00378ce3 je        0x7ff7ea288e5b
00378ce9 lea       rcx, [rsp + 0x58]
00378cee call      0x7ff7e9f43514
00378cf3 nop
00378cf4 mov       r8, r15
00378cf7 mov       rdx, qword ptr [rbp - 0x60]
00378cfb lea       rcx, [rbp + 0x68]
00378cff call      0x7ff7e9f36945
00378d04 nop
00378d05 lea       r8, [rip + 0x134b0f0]
00378d0c mov       rdx, rax
00378d0f lea       rcx, [rbp - 0x10]
00378d13 call      0x7ff7e9f42d6c
00378d18 nop
00378d19 cmp       qword ptr [rbp + 0x80], 0x10
00378d21 jb        0x7ff7ea288d2c
00378d23 mov       rcx, qword ptr [rbp + 0x68]
00378d27 call      0x7ff7eade5628
00378d2c mov       qword ptr [rbp + 0x80], 0xf
00378d37 mov       qword ptr [rbp + 0x78], r13
00378d3b mov       byte ptr [rbp + 0x68], 0
00378d3f mov       qword ptr [rbp - 0x20], 0xf
00378d47 mov       qword ptr [rbp - 0x28], r13
00378d4b mov       byte ptr [rbp - 0x38], 0
00378d4f mov       r8d, 5
00378d55 lea       rdx, [rip + 0x134a3ac]
00378d5c lea       rcx, [rbp - 0x38]
00378d60 call      0x7ff7e9f5d7c5
00378d65 nop
00378d66 lea       r9, [rbp - 0x38]
00378d6a lea       r8, [rbp - 0x10]
00378d6e lea       rdx, [rsp + 0x58]
00378d73 lea       rcx, [rip + 0x1dbbd96]
00378d7a call      0x7ff7e9f5db26 ; ?getFilesInDir@UtilityT@@QEAA_NAEAV?$lektor@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1@Z
00378d7f nop
00378d80 cmp       qword ptr [rbp - 0x20], 0x10
00378d85 jb        0x7ff7ea288d90
00378d87 mov       rcx, qword ptr [rbp - 0x38]
00378d8b call      0x7ff7eade5628
00378d90 cmp       ebx, 0x244
00378d96 mov       ebx, r13d
00378d99 jbe       0x7ff7ea288dd6
00378d9b cmp       dword ptr [rsp + 0x60], ebx
00378d9f jbe       0x7ff7ea288e04
00378da1 nop       word ptr [rax + rax]
00378db0 movsxd    rax, ebx
00378db3 lea       rcx, [rax + rax*4]
00378db7 mov       rax, qword ptr [rsp + 0x68]
00378dbc lea       r8, [rax + rcx*8]
00378dc0 lea       rdx, [rbp - 0x10]
00378dc4 mov       rcx, rdi
00378dc7 call      0x7ff7e9f4d4a1 ; ?importPlayerBuildings@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
00378dcc inc       ebx
00378dce cmp       ebx, dword ptr [rsp + 0x60]
00378dd2 jb        0x7ff7ea288db0
00378dd4 jmp       0x7ff7ea288e04
00378dd6 cmp       dword ptr [rsp + 0x60], ebx
00378dda jbe       0x7ff7ea288e04
00378ddc nop       dword ptr [rax]
00378de0 movsxd    rax, ebx
00378de3 lea       rcx, [rax + rax*4]
00378de7 mov       rax, qword ptr [rsp + 0x68]
00378dec lea       r8, [rax + rcx*8]
00378df0 lea       rdx, [rbp - 0x10]
00378df4 mov       rcx, rdi
00378df7 call      0x7ff7e9f15245 ; ?importOldPlayerBuildings@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
00378dfc inc       ebx
00378dfe cmp       ebx, dword ptr [rsp + 0x60]
00378e02 jb        0x7ff7ea288de0
00378e04 cmp       qword ptr [rbp + 8], 0x10
00378e09 jb        0x7ff7ea288e14
00378e0b mov       rcx, qword ptr [rbp - 0x10]
00378e0f call      0x7ff7eade5628
00378e14 mov       qword ptr [rbp + 8], 0xf
00378e1c mov       qword ptr [rbp], r13
00378e20 mov       byte ptr [rbp - 0x10], 0
00378e24 lea       rax, [rip + 0x1306365]
00378e2b mov       qword ptr [rsp + 0x58], rax
00378e30 mov       rcx, qword ptr [rsp + 0x68]
00378e35 test      rcx, rcx
00378e38 je        0x7ff7ea288e5b
00378e3a lea       rbx, [rcx - 8]
00378e3e lea       r9, [rip - 0x364787]
00378e45 mov       r8d, dword ptr [rbx]
00378e48 mov       edx, 0x28
00378e4d call      0x7ff7eade5cb0
00378e52 mov       rcx, rbx
00378e55 call      0x7ff7eade562e
00378e5a nop
00378e5b mov       edi, dword ptr [rsp + 0x34]
00378e5f test      dil, 0x10
00378e63 je        0x7ff7ea288e78
00378e65 call      0x7ff7e9f2c841
00378e6a mov       rcx, rax
00378e6d mov       r8b, 1
00378e70 mov       rdx, r12
00378e73 call      0x7ff7e9f2cec2
00378e78 mov       dword ptr [rsp + 0x78], r13d
00378e7d mov       r8d, 0x22
00378e83 lea       rdx, [rsp + 0x70]
00378e88 lea       rcx, [rip + 0x1dba541]
00378e8f call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00378e94 mov       r15, r13
00378e97 lea       rax, [rip + 0x1305642]
00378e9e mov       qword ptr [rsp + 0x38], rax
00378ea3 mov       dword ptr [rsp + 0x40], r13d
00378ea8 mov       ecx, 0xa
00378ead mov       dword ptr [rsp + 0x44], ecx
00378eb1 mov       qword ptr [rsp + 0x48], r13
00378eb6 mov       eax, 8
00378ebb mul       rcx
00378ebe mov       rcx, 0xffffffffffffffff
00378ec5 cmovo     rax, rcx
00378ec9 mov       rcx, rax
00378ecc call      0x7ff7eade5634
00378ed1 mov       qword ptr [rsp + 0x48], rax
00378ed6 mov       r12d, r13d
00378ed9 xorps     xmm8, xmm8
00378edd cmp       dword ptr [rsp + 0x78], r13d
00378ee2 jbe       0x7ff7ea2897dc
00378ee8 mov       r14, r13
00378eeb nop       dword ptr [rax + rax]
00378ef0 mov       rax, qword ptr [rbp - 0x80]
00378ef4 mov       rsi, qword ptr [rax + r14]
00378ef8 mov       qword ptr [rbp + 8], 0xf
00378f00 mov       qword ptr [rbp], r13
00378f04 mov       byte ptr [rbp - 0x10], 0
00378f08 mov       r8d, 0x10
00378f0e lea       rdx, [rip + 0x134a8d3]
00378f15 lea       rcx, [rbp - 0x10]
00378f19 call      0x7ff7e9f5d7c5
00378f1e nop
00378f1f lea       rcx, [rsi + 0x138]
00378f26 lea       rdx, [rbp - 0x10]
00378f2a call      0x7ff7e9f11aeb
00378f2f lea       rdx, [rax + 0x28]
00378f33 mov       rcx, qword ptr [rip + 0x1dba61e]
00378f3a call      0x7ff7e9f2bfe5 ; ?getFactionByStringID@FactionManager@@QEAAPEAVFaction@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00378f3f mov       rbx, rax
00378f42 cmp       qword ptr [rbp + 8], 0x10
00378f47 jb        0x7ff7ea288f52
00378f49 mov       rcx, qword ptr [rbp - 0x10]
00378f4d call      0x7ff7eade5628
00378f52 mov       qword ptr [rbp + 8], 0xf
00378f5a mov       qword ptr [rbp], r13
00378f5e mov       byte ptr [rbp - 0x10], 0
00378f62 test      rbx, rbx
00378f65 jne       0x7ff7ea288fa7
00378f67 lea       r8, [rsi + 0x28]
00378f6b lea       rdx, [rip + 0x134ae4e]
00378f72 lea       rcx, [rbp + 0x90]
00378f79 call      0x7ff7e9f3851f
00378f7e nop
00378f7f mov       rcx, rax
00378f82 call      0x7ff7e9f2db42 ; ?logMessageDebug@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00378f87 nop
00378f88 cmp       qword ptr [rbp + 0xa8], 0x10
00378f90 jb        0x7ff7ea2890b3
00378f96 mov       rcx, qword ptr [rbp + 0x90]
00378f9d call      0x7ff7eade5628
00378fa2 jmp       0x7ff7ea2890b3
00378fa7 cmp       qword ptr [rbx + 0x250], 0
00378faf je        0x7ff7ea2890b3
00378fb5 mov       r15, rbx
00378fb8 lea       rcx, [rsi + 0x28]
00378fbc mov       rbx, qword ptr [rcx + 0x10]
00378fc0 cmp       rbx, rbx
00378fc3 cmp       qword ptr [rcx + 0x18], 0x10
00378fc8 jb        0x7ff7ea288fcd
00378fca mov       rcx, qword ptr [rcx]
00378fcd mov       r8d, 0xe
00378fd3 cmp       rbx, r8
00378fd6 cmovb     r8, rbx
00378fda lea       rdx, [rip + 0x134adc7]
00378fe1 call      0x7ff7eade5c12
00378fe6 movsxd    rcx, eax
00378fe9 test      eax, eax
00378feb jne       0x7ff7ea289008
00378fed cmp       rbx, 0xe
00378ff1 jae       0x7ff7ea288ffc
00378ff3 or        r8, 0xffffffffffffffff
00378ff7 mov       ecx, r8d
00378ffa jmp       0x7ff7ea28900c
00378ffc mov       eax, r13d
00378fff cmp       rbx, 0xe
00379003 setne     al
00379006 mov       ecx, eax
00379008 or        r8, 0xffffffffffffffff
0037900c test      ecx, ecx
0037900e sete      al
00379011 test      al, al
00379013 sete      al
00379016 test      al, al
00379018 je        0x7ff7ea2890b3
0037901e mov       ecx, dword ptr [rsp + 0x40]
00379022 mov       eax, dword ptr [rsp + 0x44]
00379026 cmp       ecx, eax
00379028 jb        0x7ff7ea2890a4
0037902a lea       edi, [rax + rax]
0037902d test      edi, edi
0037902f mov       edx, 0xa
00379034 cmove     edi, edx
00379037 cmp       edi, eax
00379039 jbe       0x7ff7ea2890a4
0037903b mov       ecx, edi
0037903d mov       eax, 8
00379042 mul       rcx
00379045 cmovo     rax, r8
00379049 mov       rcx, rax
0037904c call      0x7ff7eade5634
00379051 mov       rbx, rax
00379054 mov       r9d, r13d
00379057 cmp       dword ptr [rsp + 0x40], r13d
0037905c jbe       0x7ff7ea28908b
0037905e mov       r8, r13
00379061 nop       word ptr [rax + rax]
00379070 mov       rcx, qword ptr [rsp + 0x48]
00379075 mov       rdx, qword ptr [rcx + r8]
00379079 mov       qword ptr [r8 + rax], rdx
0037907d inc       r9d
00379080 add       r8, 8
00379084 cmp       r9d, dword ptr [rsp + 0x40]
00379089 jb        0x7ff7ea289070
0037908b mov       rcx, qword ptr [rsp + 0x48]
00379090 call      0x7ff7eade562e
00379095 mov       qword ptr [rsp + 0x48], rbx
0037909a mov       dword ptr [rsp + 0x44], edi
0037909e mov       ecx, dword ptr [rsp + 0x40]
003790a2 jmp       0x7ff7ea2890a9
003790a4 mov       rbx, qword ptr [rsp + 0x48]
003790a9 mov       eax, ecx
003790ab mov       qword ptr [rbx + rax*8], rsi
003790af inc       dword ptr [rsp + 0x40]
003790b3 inc       r12d
003790b6 add       r14, 8
003790ba cmp       r12d, dword ptr [rsp + 0x78]
003790bf jb        0x7ff7ea288ef0
003790c5 test      r15, r15
003790c8 je        0x7ff7ea2897d3
003790ce mov       esi, r13d
003790d1 cmp       dword ptr [rsp + 0x40], r13d
003790d6 jbe       0x7ff7ea2891db
003790dc mov       r12d, dword ptr [rsp + 0x34]
003790e1 and       r12d, 1
003790e5 mov       rdi, r13
003790e8 movss     xmm6, dword ptr [rip + 0x134acb4]
003790f0 movss     xmm7, dword ptr [rip + 0x1313bfc]
003790f8 nop       dword ptr [rax + rax]
00379100 mov       qword ptr [rbp - 0x20], 0xf
00379108 mov       qword ptr [rbp - 0x28], r13
0037910c mov       byte ptr [rbp - 0x38], 0
00379110 mov       r8d, 0xb
00379116 lea       rdx, [rip + 0x134a5db]
0037911d lea       rcx, [rbp - 0x38]
00379121 call      0x7ff7e9f5d7c5
00379126 nop
00379127 mov       rax, qword ptr [rsp + 0x48]
0037912c mov       rcx, qword ptr [rax + rdi]
00379130 add       rcx, 0x178
00379137 lea       rdx, [rbp - 0x38]
0037913b call      0x7ff7e9f4b043
00379140 nop
00379141 cmp       qword ptr [rbp - 0x20], 0x10
00379146 jb        0x7ff7ea289151
00379148 mov       rcx, qword ptr [rbp - 0x38]
0037914c call      0x7ff7eade5628
00379151 test      r12d, r12d
00379154 je        0x7ff7ea2891cb
00379156 mov       qword ptr [rbp - 0x20], 0xf
0037915e mov       qword ptr [rbp - 0x28], r13
00379162 mov       byte ptr [rbp - 0x38], 0
00379166 mov       r8d, 8
0037916c lea       rdx, [rip + 0x130453d]
00379173 lea       rcx, [rbp - 0x38]
00379177 call      0x7ff7e9f5d7c5
0037917c nop
0037917d mov       rax, qword ptr [rsp + 0x48]
00379182 mov       rcx, qword ptr [rax + rdi]
00379186 add       rcx, 0x238
0037918d lea       rdx, [rbp - 0x38]
00379191 call      0x7ff7e9f46dd6
00379196 mov       rbx, rax
00379199 movaps    xmm3, xmm6
0037919c movaps    xmm2, xmm8
003791a0 movaps    xmm1, xmm7
003791a3 lea       rcx, [rbp - 0x50]
003791a7 call      qword ptr [rip + 0x1ecd02b]
003791ad mov       rdx, rax
003791b0 lea       rcx, [rbx + 0x28]
003791b4 call      qword ptr [rip + 0x1ecd026]
003791ba nop
003791bb cmp       qword ptr [rbp - 0x20], 0x10
003791c0 jb        0x7ff7ea2891cb
003791c2 mov       rcx, qword ptr [rbp - 0x38]
003791c6 call      0x7ff7eade5628
003791cb inc       esi
003791cd add       rdi, 8
003791d1 cmp       esi, dword ptr [rsp + 0x40]
003791d5 jb        0x7ff7ea289100
003791db lea       rdx, [rsp + 0x38]
003791e0 mov       rcx, r15
003791e3 call      0x7ff7e9f602d6 ; ?createPlatoonsAuto@Faction@@QEAAXAEAV?$lektor@PEAVGameData@@@@@Z
003791e8 mov       r12d, r13d
003791eb mov       dword ptr [rbp - 0x78], r13d
003791ef cmp       dword ptr [rsp + 0x40], r13d
003791f4 jbe       0x7ff7ea2897d3
003791fa mov       r15, r13
003791fd mov       qword ptr [rbp - 0x50], r13
00379201 nop       word ptr [rax + rax]
00379210 mov       qword ptr [rbp + 0x58], 0xf
00379218 mov       qword ptr [rbp + 0x50], r13
0037921c mov       byte ptr [rbp + 0x40], 0
00379220 mov       r8d, 0xc
00379226 lea       rdx, [rip + 0x134a4eb]
0037922d lea       rcx, [rbp + 0x40]
00379231 call      0x7ff7e9f5d7c5
00379236 nop
00379237 mov       rax, qword ptr [rsp + 0x48]
0037923c mov       rcx, qword ptr [rax + r15]
00379240 add       rcx, 0x1f8
00379247 lea       rdx, [rbp + 0x40]
0037924b call      0x7ff7e9f11aeb
00379250 lea       rdx, [rax + 0x28]
00379254 lea       rcx, [rbp + 0x90]
0037925b call      0x7ff7e9f309fa ; ?removePathFromString@UtilityT@@SA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
00379260 nop
00379261 cmp       qword ptr [rbp + 0x58], 0x10
00379266 jb        0x7ff7ea289271
00379268 mov       rcx, qword ptr [rbp + 0x40]
0037926c call      0x7ff7eade5628
00379271 mov       qword ptr [rbp + 0x58], 0xf
00379279 mov       qword ptr [rbp + 0x50], r13
0037927d mov       byte ptr [rbp + 0x40], 0
00379281 mov       r8, qword ptr [rbp - 0x58]
00379285 mov       rdx, qword ptr [rbp - 0x60]
00379289 lea       rcx, [rbp + 0x160]
00379290 call      0x7ff7e9f36945
00379295 nop
00379296 lea       r8, [rip + 0x134aafb]
0037929d mov       rdx, rax
003792a0 lea       rcx, [rbp - 0x38]
003792a4 call      0x7ff7e9f42d6c
003792a9 nop
003792aa lea       r8, [rbp + 0x90]
003792b1 mov       rdx, rax
003792b4 lea       rcx, [rbp - 0x10]
003792b8 call      0x7ff7e9f49c1b
003792bd nop
003792be cmp       qword ptr [rbp - 0x20], 0x10
003792c3 jb        0x7ff7ea2892ce
003792c5 mov       rcx, qword ptr [rbp - 0x38]
003792c9 call      0x7ff7eade5628
003792ce mov       qword ptr [rbp - 0x20], 0xf
003792d6 mov       qword ptr [rbp - 0x28], r13
003792da mov       byte ptr [rbp - 0x38], 0
003792de cmp       qword ptr [rbp + 0x178], 0x10
003792e6 jb        0x7ff7ea2892f4
003792e8 mov       rcx, qword ptr [rbp + 0x160]
003792ef call      0x7ff7eade5628
003792f4 mov       qword ptr [rbp + 0x178], 0xf
003792ff mov       qword ptr [rbp + 0x170], r13
00379306 mov       byte ptr [rbp + 0x160], 0
0037930d lea       r8, [rbp + 0x90]
00379314 lea       rdx, [rip + 0x1349d65]
0037931b lea       rcx, [rbp + 0x18]
0037931f call      0x7ff7e9f3851f
00379324 mov       rbx, rax
00379327 mov       ecx, dword ptr [rip + 0x1db48e7]
0037932d test      cl, 1
00379330 jne       0x7ff7ea289365
00379332 or        ecx, 1
00379335 mov       dword ptr [rip + 0x1db48d9], ecx
0037933b mov       ecx, 0x318
00379340 call      0x7ff7eade563a
00379345 mov       qword ptr [rsp + 0x50], rax
0037934a test      rax, rax
0037934d je        0x7ff7ea289359
0037934f mov       rcx, rax
00379352 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00379357 jmp       0x7ff7ea28935c
00379359 mov       rax, r13
0037935c mov       qword ptr [rip + 0x1db48a5], rax
00379363 jmp       0x7ff7ea28936c
00379365 mov       rax, qword ptr [rip + 0x1db489c]
0037936c mov       r8, rbx
0037936f lea       rdx, [rbp + 0x68]
00379373 mov       rcx, rax
00379376 call      0x7ff7e9f461ce ; ?writeFile@SaveFileSystem@@QEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
0037937b nop
0037937c cmp       qword ptr [rbp + 0x30], 0x10
00379381 jb        0x7ff7ea28938c
00379383 mov       rcx, qword ptr [rbp + 0x18]
00379387 call      0x7ff7eade5628
0037938c mov       qword ptr [rbp + 0x30], 0xf
00379394 mov       qword ptr [rbp + 0x28], r13
00379398 mov       byte ptr [rbp + 0x18], 0
0037939c lea       r8, [rbp + 0x68]
003793a0 lea       rdx, [rbp - 0x10]
003793a4 lea       rcx, [rip + 0x1dbb765]
003793ab call      0x7ff7e9f489a1 ; ?copyFile@UtilityT@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
003793b0 test      al, al
003793b2 jne       0x7ff7ea2894af
003793b8 mov       qword ptr [rbp + 0x58], 7
003793c0 mov       qword ptr [rbp + 0x50], r13
003793c4 mov       word ptr [rbp + 0x40], r13w
003793c9 xor       r8d, r8d
003793cc mov       rdx, qword ptr [rbp]
003793d0 lea       rcx, [rbp + 0x40]
003793d4 call      0x7ff7e9f16cda
003793d9 nop
003793da lea       rcx, [rbp + 0x40]
003793de cmp       qword ptr [rbp + 0x58], 8
003793e3 cmovae    rcx, qword ptr [rbp + 0x40]
003793e8 lea       r8, [rbp - 0x10]
003793ec cmp       qword ptr [rbp + 8], 0x10
003793f1 cmovae    r8, qword ptr [rbp - 0x10]
003793f6 mov       eax, dword ptr [rbp + 0x50]
003793f9 mov       dword ptr [rsp + 0x28], eax
003793fd mov       qword ptr [rsp + 0x20], rcx
00379402 mov       r9d, dword ptr [rbp]
00379406 mov       edx, 1
0037940b xor       ecx, ecx
0037940d call      qword ptr [rip + 0x1eca62d]
00379413 mov       eax, 7
00379418 mov       qword ptr [rbp - 0x20], rax
0037941c mov       qword ptr [rbp - 0x28], r13
00379420 mov       word ptr [rbp - 0x38], r13w
00379425 mov       rsi, qword ptr [rbp + 0x78]
00379429 test      rsi, rsi
0037942c je        0x7ff7ea289469
0037942e lea       rdi, [rbp + 0x68]
00379432 mov       rcx, qword ptr [rbp + 0x68]
00379436 mov       rax, qword ptr [rbp + 0x80]
0037943d cmp       rax, 0x10
00379441 cmovae    rdi, rcx
00379445 lea       rbx, [rbp + 0x68]
00379449 cmovae    rbx, rcx
0037944d call      0x7ff7eadefc90
00379452 mov       r9, rax
00379455 lea       rdx, [rdi + rsi]
00379459 lea       r8, [rbp - 0x38]
0037945d mov       rcx, rbx
00379460 call      0x7ff7eadef440
00379465 mov       rax, qword ptr [rbp - 0x20]
00379469 lea       rdx, [rbp - 0x38]
0037946d cmp       rax, 8
00379471 cmovae    rdx, qword ptr [rbp - 0x38]
00379476 lea       rcx, [rbp + 0x40]
0037947a cmp       qword ptr [rbp + 0x58], 8
0037947f cmovae    rcx, qword ptr [rbp + 0x40]
00379484 xor       r8d, r8d
00379487 call      qword ptr [rip + 0x1eca7d3]
0037948d cmp       qword ptr [rbp - 0x20], 8
00379492 jb        0x7ff7ea28949e
00379494 mov       rcx, qword ptr [rbp - 0x38]
00379498 call      0x7ff7eade5628
0037949d nop
0037949e cmp       qword ptr [rbp + 0x58], 8
003794a3 jb        0x7ff7ea2894af
003794a5 mov       rcx, qword ptr [rbp + 0x40]
003794a9 call      0x7ff7eade5628
003794ae nop
003794af lea       rax, [rip + 0x130502a]
003794b6 mov       qword ptr [rsp + 0x58], rax
003794bb mov       dword ptr [rsp + 0x60], r13d
003794c0 mov       ecx, 0xa
003794c5 mov       dword ptr [rsp + 0x64], ecx
003794c9 mov       qword ptr [rsp + 0x68], r13
003794ce mov       eax, 8
003794d3 mul       rcx
003794d6 mov       rbx, 0xffffffffffffffff
003794dd cmovo     rax, rbx
003794e1 mov       rcx, rax
003794e4 call      0x7ff7eade5634
003794e9 mov       qword ptr [rsp + 0x68], rax
003794ee lea       rcx, [rbp + 0x190]
003794f5 call      0x7ff7e9f63c92 ; ??0GameDataContainer@@QEAA@XZ | ?_CONSTRUCTOR@GameDataContainer@@QEAAPEAV1@XZ
003794fa nop
003794fb mov       byte ptr [rsp + 0x28], 0
00379500 mov       qword ptr [rsp + 0x20], r13
00379505 mov       r9d, ebx
00379508 lea       r8, [rip + 0x1bfeb89]
0037950f lea       rdx, [rbp + 0x68]
00379513 lea       rcx, [rbp + 0x190]
0037951a call      0x7ff7e9f53ed7 ; ?load@GameDataContainer@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0HPEAVSerialisable@@_N@Z
0037951f test      al, al
00379521 je        0x7ff7ea289722
00379527 lea       r8d, [rbx + 0x1f]
0037952b lea       rdx, [rsp + 0x58]
00379530 lea       rcx, [rbp + 0x190]
00379537 call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
0037953c mov       r11, qword ptr [rsp + 0x68]
00379541 mov       rax, qword ptr [r11]
00379544 mov       rbx, qword ptr [rax + 0x90]
0037954b mov       rbx, qword ptr [rbx]
0037954e mov       r14d, dword ptr [rsp + 0x30]
00379553 cmp       rbx, qword ptr [rax + 0x90]
0037955a je        0x7ff7ea289727
00379560 lea       r12, [rip + 0x13016a1]
00379567 lea       r15, [rip + 0x1345232]
0037956e nop
00379570 lea       rdx, [rbx + 0x60]
00379574 mov       r8d, 1
0037957a lea       rcx, [rip + 0x1db9b4f]
00379581 call      0x7ff7e9f51aa6 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4itemType@@@Z
00379586 mov       rsi, rax
00379589 test      rax, rax
0037958c je        0x7ff7ea2895d9
0037958e mov       qword ptr [rbp - 0x20], 0xf
00379596 mov       qword ptr [rbp - 0x28], r13
0037959a mov       byte ptr [rbp - 0x38], 0
0037959e mov       r8d, 6
003795a4 lea       rdx, [rip + 0x134711d]
003795ab lea       rcx, [rbp - 0x38]
003795af call      0x7ff7e9f5d7c5
003795b4 nop
003795b5 or        r14d, 1
003795b9 mov       dword ptr [rsp + 0x50], r14d
003795be lea       rcx, [rsi + 0xf8]
003795c5 lea       rdx, [rbp - 0x38]
003795c9 call      0x7ff7e9f5f886
003795ce cmp       byte ptr [rax + 0x28], 0
003795d2 je        0x7ff7ea2895d9
003795d4 mov       dil, 1
003795d7 jmp       0x7ff7ea2895dc
003795d9 xor       dil, dil
003795dc test      r14b, 1
003795e0 je        0x7ff7ea2895f6
003795e2 and       r14d, 0xfffffffe
003795e6 cmp       qword ptr [rbp - 0x20], 0x10
003795eb jb        0x7ff7ea2895f6
003795ed mov       rcx, qword ptr [rbp - 0x38]
003795f1 call      0x7ff7eade5628
003795f6 test      dil, dil
003795f9 je        0x7ff7ea289692
003795ff mov       eax, dword ptr [rip + 0x1db44f7]
00379605 test      al, 1
00379607 jne       0x7ff7ea289658
00379609 or        eax, 1
0037960c mov       dword ptr [rip + 0x1db44ea], eax
00379612 mov       ecx, 0x40
00379617 call      0x7ff7eade563a
0037961c mov       rdi, rax
0037961f test      rax, rax
00379622 je        0x7ff7ea28964c
00379624 mov       byte ptr [rax], 0
00379627 mov       qword ptr [rax + 8], r12
0037962b mov       qword ptr [rax + 0x10], r15
0037962f mov       qword ptr [rax + 0x18], 0x10
00379637 mov       qword ptr [rax + 0x20], r13
0037963b mov       dword ptr [rax + 0x28], 0x3f800000
00379642 mov       qword ptr [rax + 0x30], r13
00379646 mov       qword ptr [rax + 0x38], r13
0037964a jmp       0x7ff7ea28964f
0037964c mov       rdi, r13
0037964f mov       qword ptr [rip + 0x1db449a], rdi
00379656 jmp       0x7ff7ea28965f
00379658 mov       rdi, qword ptr [rip + 0x1db4491]
0037965f mov       qword ptr [rsp + 0x50], rsi
00379664 lea       rdx, [rsp + 0x50]
00379669 mov       rcx, rdi
0037966c call      0x7ff7e9f3d17d
00379671 mov       qword ptr [rax + 8], rsi
00379675 lea       rdx, [rsp + 0x50]
0037967a mov       rcx, rdi
0037967d call      0x7ff7e9f3d17d
00379682 mov       qword ptr [rax + 0x18], 0xb
0037968a mov       qword ptr [rax + 0x20], r13
0037968e mov       dword ptr [rax + 0x28], r13d
00379692 cmp       byte ptr [rbx + 0xa9], 0
00379699 jne       0x7ff7ea2896f9
0037969b mov       rax, qword ptr [rbx + 0x10]
0037969f cmp       byte ptr [rax + 0xa9], 0
003796a6 jne       0x7ff7ea2896d1
003796a8 mov       rbx, rax
003796ab mov       rax, qword ptr [rax]
003796ae cmp       byte ptr [rax + 0xa9], 0
003796b5 jne       0x7ff7ea2896f9
003796b7 nop       word ptr [rax + rax]
003796c0 mov       rbx, rax
003796c3 mov       rax, qword ptr [rax]
003796c6 cmp       byte ptr [rax + 0xa9], 0
003796cd je        0x7ff7ea2896c0
003796cf jmp       0x7ff7ea2896f9
003796d1 mov       rax, qword ptr [rbx + 8]
003796d5 cmp       byte ptr [rax + 0xa9], 0
003796dc jne       0x7ff7ea2896f6
003796de nop
003796e0 cmp       rbx, qword ptr [rax + 0x10]
003796e4 jne       0x7ff7ea2896f6
003796e6 mov       rbx, rax
003796e9 mov       rax, qword ptr [rax + 8]
003796ed cmp       byte ptr [rax + 0xa9], 0
003796f4 je        0x7ff7ea2896e0
003796f6 mov       rbx, rax
003796f9 mov       rax, qword ptr [rsp + 0x68]
003796fe mov       rcx, qword ptr [rax]
00379701 cmp       rbx, qword ptr [rcx + 0x90]
00379708 jne       0x7ff7ea289570
0037970e mov       dword ptr [rsp + 0x30], r14d
00379713 mov       dword ptr [rsp + 0x30], r14d
00379718 mov       r12d, dword ptr [rbp - 0x78]
0037971c mov       r15, qword ptr [rbp - 0x50]
00379720 jmp       0x7ff7ea289727
00379722 mov       r14d, dword ptr [rsp + 0x30]
00379727 lea       rcx, [rbp + 0x190]
0037972e call      0x7ff7e9f61b7c ; ??1GameDataContainer@@UEAA@XZ | ?_DESTRUCTOR@GameDataContainer@@QEAAXXZ
00379733 nop
00379734 lea       rax, [rip + 0x1304da5]
0037973b mov       qword ptr [rsp + 0x58], rax
00379740 mov       rcx, qword ptr [rsp + 0x68]
00379745 test      rcx, rcx
00379748 je        0x7ff7ea28974f
0037974a call      0x7ff7eade562e
0037974f lea       rax, [rip + 0x1304ce2]
00379756 mov       qword ptr [rsp + 0x58], rax
0037975b cmp       qword ptr [rbp + 0x80], 0x10
00379763 jb        0x7ff7ea28976e
00379765 mov       rcx, qword ptr [rbp + 0x68]
00379769 call      0x7ff7eade5628
0037976e mov       qword ptr [rbp + 0x80], 0xf
00379779 mov       qword ptr [rbp + 0x78], r13
0037977d mov       byte ptr [rbp + 0x68], 0
00379781 cmp       qword ptr [rbp + 8], 0x10
00379786 jb        0x7ff7ea289791
00379788 mov       rcx, qword ptr [rbp - 0x10]
0037978c call      0x7ff7eade5628
00379791 mov       qword ptr [rbp + 8], 0xf
00379799 mov       qword ptr [rbp], r13
0037979d mov       byte ptr [rbp - 0x10], 0
003797a1 cmp       qword ptr [rbp + 0xa8], 0x10
003797a9 jb        0x7ff7ea2897b7
003797ab mov       rcx, qword ptr [rbp + 0x90]
003797b2 call      0x7ff7eade5628
003797b7 inc       r12d
003797ba mov       dword ptr [rbp - 0x78], r12d
003797be add       r15, 8
003797c2 mov       qword ptr [rbp - 0x50], r15
003797c6 cmp       r12d, dword ptr [rsp + 0x40]
003797cb jb        0x7ff7ea289210
003797d1 jmp       0x7ff7ea2897d8
003797d3 mov       r14d, dword ptr [rsp + 0x30]
003797d8 mov       edi, dword ptr [rsp + 0x34]
003797dc mov       ebx, r13d
003797df mov       rcx, qword ptr [rip + 0x1db9e4a]
003797e6 call      0x7ff7e9f1fb3c ; ?getCurrentPlatoon@PlayerInterface@@QEAAPEAVPlatoon@@XZ
003797eb test      rax, rax
003797ee je        0x7ff7ea289809
003797f0 mov       rcx, qword ptr [rip + 0x1db9e39]
003797f7 call      0x7ff7e9f1fb3c ; ?getCurrentPlatoon@PlayerInterface@@QEAAPEAVPlatoon@@XZ
003797fc cmp       qword ptr [rax + 0x1d8], rbx
00379803 jne       0x7ff7ea28994f
00379809 mov       r8, qword ptr [rbp - 0x58]
0037980d lea       rdx, [rip + 0x134a578]
00379814 lea       rcx, [rbp + 0x90]
0037981b call      0x7ff7e9f3851f
00379820 nop
00379821 lea       r8, [rip + 0x134a540]
00379828 mov       rdx, rax
0037982b lea       rcx, [rbp + 0x18]
0037982f call      0x7ff7e9f42d6c
00379834 nop
00379835 mov       rcx, qword ptr [rip + 0x1db9df4]
0037983c mov       rdx, rax
0037983f mov       rcx, qword ptr [rcx + 0x2a0]
00379846 call      0x7ff7e9f3a19e ; ?createReplacementPlatoonForPlayerWhenSavegameIsCorrupt@Faction@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
0037984b nop
0037984c cmp       qword ptr [rbp + 0x30], 0x10
00379851 jb        0x7ff7ea28985c
00379853 mov       rcx, qword ptr [rbp + 0x18]
00379857 call      0x7ff7eade5628
0037985c mov       qword ptr [rbp + 0x30], 0xf
00379864 mov       qword ptr [rbp + 0x28], r13
00379868 mov       byte ptr [rbp + 0x18], 0
0037986c cmp       qword ptr [rbp + 0xa8], 0x10
00379874 jb        0x7ff7ea289882
00379876 mov       rcx, qword ptr [rbp + 0x90]
0037987d call      0x7ff7eade5628
00379882 lea       rdx, [rip + 0x134a477]
00379889 lea       rcx, [rbp + 0xc0]
00379890 call      0x7ff7e9f210b8
00379895 nop
00379896 lea       rdx, [rbp + 0x18]
0037989a mov       rcx, rax
0037989d call      0x7ff7e9f18a3a
003798a2 nop
003798a3 mov       r8b, 1
003798a6 mov       rdx, rax
003798a9 lea       rcx, [rip + 0x1db9800]
003798b0 call      0x7ff7e9f3b1ac ; ?showPlayerAMessage@GameWorld@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z
003798b5 nop
003798b6 cmp       qword ptr [rbp + 0x30], 0x10
003798bb jb        0x7ff7ea2898c6
003798bd mov       rcx, qword ptr [rbp + 0x18]
003798c1 call      0x7ff7eade5628
003798c6 mov       qword ptr [rbp + 0x30], 0xf
003798ce mov       qword ptr [rbp + 0x28], r13
003798d2 mov       byte ptr [rbp + 0x18], 0
003798d6 cmp       qword ptr [rbp + 0x148], 0x10
003798de jb        0x7ff7ea2898ec
003798e0 mov       rcx, qword ptr [rbp + 0x130]
003798e7 call      0x7ff7eade5628
003798ec mov       qword ptr [rbp + 0x148], 0xf
003798f7 mov       qword ptr [rbp + 0x140], r13
003798fe mov       byte ptr [rbp + 0x130], 0
00379905 cmp       qword ptr [rbp + 0x120], 0x10
0037990d jb        0x7ff7ea28991b
0037990f mov       rcx, qword ptr [rbp + 0x108]
00379916 call      0x7ff7eade5628
0037991b mov       qword ptr [rbp + 0x120], 0xf
00379926 mov       qword ptr [rbp + 0x118], r13
0037992d mov       byte ptr [rbp + 0x108], 0
00379934 cmp       qword ptr [rbp + 0xf8], 0x10
0037993c jb        0x7ff7ea28994a
0037993e mov       rcx, qword ptr [rbp + 0xe0]
00379945 call      0x7ff7eade5628
0037994a mov       ebx, 0x2710
0037994f mov       dword ptr [rsp + 0x78], r13d
00379954 test      dil, 8
00379958 je        0x7ff7ea289995
0037995a mov       r8d, 0x15
00379960 lea       rdx, [rsp + 0x70]
00379965 lea       rcx, [rip + 0x1db9a64]
0037996c call      0x7ff7e9f60763 ; ?getDataOfType@GameDataContainer@@QEAAXAEAV?$lektor@PEAVGameData@@@@W4itemType@@@Z
00379971 cmp       dword ptr [rsp + 0x78], 0
00379976 jbe       0x7ff7ea289a06
0037997c mov       rax, qword ptr [rbp - 0x80]
00379980 mov       rdx, qword ptr [rax]
00379983 mov       rcx, qword ptr [rip + 0x1db9ca6]
0037998a mov       rcx, qword ptr [rcx + 0x38]
0037998e call      0x7ff7e9f316e8 ; ?load@Research@@QEAAXPEAVGameData@@@Z
00379993 jmp       0x7ff7ea289a06
00379995 mov       rcx, qword ptr [rip + 0x1db9c94]
0037999c mov       rcx, qword ptr [rcx + 0x38]
003799a0 call      0x7ff7e9f41c5a ; ?_reset@Research@@QEAAXXZ
003799a5 mov       qword ptr [rbp - 0x20], 0xf
003799ad mov       qword ptr [rbp - 0x28], r13
003799b1 mov       byte ptr [rbp - 0x38], 0
003799b5 mov       r8d, 0xe
003799bb lea       rdx, [rip + 0x134a31e]
003799c2 lea       rcx, [rbp - 0x38]
003799c6 call      0x7ff7e9f5d7c5
003799cb nop
003799cc mov       r8d, 0x15
003799d2 lea       rdx, [rbp - 0x38]
003799d6 lea       rcx, [rip + 0x1db96f3]
003799dd call      0x7ff7e9f183b9 ; ?getDataByName@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4itemType@@@Z
003799e2 mov       rdx, rax
003799e5 mov       rcx, qword ptr [rip + 0x1db9c44]
003799ec mov       rcx, qword ptr [rcx + 0x38]
003799f0 call      0x7ff7e9f112fd ; ?completeResearch@Research@@QEAAXPEAVGameData@@@Z
003799f5 nop
003799f6 cmp       qword ptr [rbp - 0x20], 0x10
003799fb jb        0x7ff7ea289a06
003799fd mov       rcx, qword ptr [rbp - 0x38]
00379a01 call      0x7ff7eade5628
00379a06 mov       dword ptr [rsp + 0x78], r13d
00379a0b mov       rsi, qword ptr [rbp - 0x70]
00379a0f test      rsi, rsi
00379a12 je        0x7ff7ea289b1c
00379a18 mov       qword ptr [rbp - 0x20], 0xf
00379a20 mov       qword ptr [rbp - 0x28], r13
00379a24 mov       byte ptr [rbp - 0x38], 0
00379a28 mov       r8d, 0xc
00379a2e lea       rdx, [rip + 0x1349ffb]
00379a35 lea       rcx, [rbp - 0x38]
00379a39 call      0x7ff7e9f5d7c5
00379a3e nop
00379a3f lea       rcx, [rsi + 0x178]
00379a46 lea       rdx, [rbp - 0x38]
00379a4a call      0x7ff7e9f580fe
00379a4f mov       edi, dword ptr [rax + 0x28]
00379a52 cmp       qword ptr [rbp - 0x20], 0x10
00379a57 jb        0x7ff7ea289a62
00379a59 mov       rcx, qword ptr [rbp - 0x38]
00379a5d call      0x7ff7eade5628
00379a62 mov       eax, r13d
00379a65 test      edi, edi
00379a67 cmovg     eax, edi
00379a6a add       ebx, eax
00379a6c mov       rax, qword ptr [rip + 0x1db9bbd]
00379a73 mov       rdx, qword ptr [rax + 0x2a0]
00379a7a mov       rax, qword ptr [rdx + 0x80]
00379a81 mov       dword ptr [rax + 0x88], ebx
00379a87 mov       qword ptr [rbp - 0x20], 0xf
00379a8f mov       qword ptr [rbp - 0x28], r13
00379a93 mov       byte ptr [rbp - 0x38], 0
00379a97 mov       r8d, 0xd
00379a9d lea       rdx, [rip + 0x1349fac]
00379aa4 lea       rcx, [rbp - 0x38]
00379aa8 call      0x7ff7e9f5d7c5
00379aad nop
00379aae lea       rdx, [rbp - 0x38]
00379ab2 lea       rcx, [rsi + 0x138]
00379ab9 call      0x7ff7e9f11aeb
00379abe lea       rdx, [rax + 0x28]
00379ac2 mov       rcx, qword ptr [rip + 0x1db9b67]
00379ac9 add       rcx, 8
00379acd or        r12, 0xffffffffffffffff
00379ad1 mov       r9, r12
00379ad4 xor       r8d, r8d
00379ad7 call      0x7ff7e9f3808d
00379adc nop
00379add cmp       qword ptr [rbp - 0x20], 0x10
00379ae2 jb        0x7ff7ea289aed
00379ae4 mov       rcx, qword ptr [rbp - 0x38]
00379ae8 call      0x7ff7eade5628
00379aed mov       rdx, rsi
00379af0 mov       rcx, qword ptr [rip + 0x1db9b39]
00379af7 mov       rcx, qword ptr [rcx + 0x30]
00379afb call      0x7ff7e9f31567 ; ?load@CameraClass@@QEAAXPEAVGameData@@@Z
00379b00 mov       rcx, qword ptr [rip + 0x1db9b29]
00379b07 add       rcx, 0x110
00379b0e mov       rdx, rsi
00379b11 call      0x7ff7e9f2418c ; ?load@AIOptions@PlayerInterface@@QEAAXPEAVGameData@@@Z
00379b16 mov       edi, dword ptr [rsp + 0x34]
00379b1a jmp       0x7ff7ea289b20
00379b1c or        r12, 0xffffffffffffffff
00379b20 mov       rdx, qword ptr [rip + 0x1db9b09]
00379b27 cmp       qword ptr [rdx + 0x18], 0
00379b2c jne       0x7ff7ea289ce7
00379b32 mov       qword ptr [rbp + 8], 0xf
00379b3a mov       qword ptr [rbp], r13
00379b3e mov       byte ptr [rbp - 0x10], 0
00379b42 mov       r8d, 0x11
00379b48 lea       rdx, [rip + 0x13496d9]
00379b4f lea       rcx, [rbp - 0x10]
00379b53 call      0x7ff7e9f5d7c5
00379b58 nop
00379b59 lea       rdx, [rbp - 0x10]
00379b5d lea       rcx, [rip + 0x1db956c]
00379b64 call      0x7ff7e9f24790 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
00379b69 mov       rbx, rax
00379b6c cmp       qword ptr [rbp + 8], 0x10
00379b71 jb        0x7ff7ea289b7c
00379b73 mov       rcx, qword ptr [rbp - 0x10]
00379b77 call      0x7ff7eade5628
00379b7c mov       qword ptr [rbp + 8], 0xf
00379b84 mov       qword ptr [rbp], r13
00379b88 mov       byte ptr [rbp - 0x10], 0
00379b8c test      rbx, rbx
00379b8f je        0x7ff7ea289bbe
00379b91 mov       qword ptr [rbp - 0x20], 0xf
00379b99 mov       qword ptr [rbp - 0x28], r13
00379b9d mov       byte ptr [rbp - 0x38], 0
00379ba1 lea       rdx, [rbx + 0x28]
00379ba5 mov       r9, r12
00379ba8 xor       r8d, r8d
00379bab lea       rcx, [rbp - 0x38]
00379baf call      0x7ff7e9f3808d
00379bb4 lea       rax, [rbp - 0x38]
00379bb8 or        r14d, 2
00379bbc jmp       0x7ff7ea289bec
00379bbe lea       rdx, [rip + 0x1349653]
00379bc5 lea       rcx, [rbp + 0xc0]
00379bcc call      0x7ff7e9f210b8
00379bd1 nop
00379bd2 or        r14d, 4
00379bd6 mov       dword ptr [rsp + 0x50], r14d
00379bdb lea       rdx, [rbp + 0x18]
00379bdf mov       rcx, rax
00379be2 call      0x7ff7e9f18a3a
00379be7 nop
00379be8 or        r14d, 8
00379bec mov       dword ptr [rsp + 0x50], r14d
00379bf1 mov       rcx, qword ptr [rip + 0x1db9a38]
00379bf8 add       rcx, 8
00379bfc mov       rdx, rax
00379bff call      0x7ff7e9f121a3
00379c04 nop
00379c05 test      r14b, 8
00379c09 je        0x7ff7ea289c2f
00379c0b and       r14d, 0xfffffff7
00379c0f cmp       qword ptr [rbp + 0x30], 0x10
00379c14 jb        0x7ff7ea289c1f
00379c16 mov       rcx, qword ptr [rbp + 0x18]
00379c1a call      0x7ff7eade5628
00379c1f mov       qword ptr [rbp + 0x30], 0xf
00379c27 mov       qword ptr [rbp + 0x28], r13
00379c2b mov       byte ptr [rbp + 0x18], 0
00379c2f test      r14b, 4
00379c33 je        0x7ff7ea289cca
00379c39 and       r14d, 0xfffffffb
00379c3d cmp       qword ptr [rbp + 0x148], 0x10
00379c45 jb        0x7ff7ea289c53
00379c47 mov       rcx, qword ptr [rbp + 0x130]
00379c4e call      0x7ff7eade5628
00379c53 mov       qword ptr [rbp + 0x148], 0xf
00379c5e mov       qword ptr [rbp + 0x140], r13
00379c65 mov       byte ptr [rbp + 0x130], 0
00379c6c cmp       qword ptr [rbp + 0x120], 0x10
00379c74 jb        0x7ff7ea289c82
00379c76 mov       rcx, qword ptr [rbp + 0x108]
00379c7d call      0x7ff7eade5628
00379c82 mov       qword ptr [rbp + 0x120], 0xf
00379c8d mov       qword ptr [rbp + 0x118], r13
00379c94 mov       byte ptr [rbp + 0x108], 0
00379c9b cmp       qword ptr [rbp + 0xf8], 0x10
00379ca3 jb        0x7ff7ea289cb1
00379ca5 mov       rcx, qword ptr [rbp + 0xe0]
00379cac call      0x7ff7eade5628
00379cb1 mov       qword ptr [rbp + 0xf8], 0xf
00379cbc mov       qword ptr [rbp + 0xf0], r13
00379cc3 mov       byte ptr [rbp + 0xe0], 0
00379cca test      r14b, 2
00379cce je        0x7ff7ea289ce0
00379cd0 cmp       qword ptr [rbp - 0x20], 0x10
00379cd5 jb        0x7ff7ea289ce0
00379cd7 mov       rcx, qword ptr [rbp - 0x38]
00379cdb call      0x7ff7eade5628
00379ce0 mov       rdx, qword ptr [rip + 0x1db9949]
00379ce7 mov       rcx, qword ptr [rdx + 0x2a0]
00379cee add       rdx, 8
00379cf2 add       rcx, 0x1a8
00379cf9 mov       r9, r12
00379cfc xor       r8d, r8d
00379cff call      0x7ff7e9f3808d
00379d04 test      dil, 1
00379d08 je        0x7ff7ea289d6a
00379d0a movss     xmm7, dword ptr [rip + 0x1349fca]
00379d12 movaps    xmm3, xmm7
00379d15 movaps    xmm2, xmm8
00379d19 movss     xmm6, dword ptr [rip + 0x1349fb7]
00379d21 movaps    xmm1, xmm6
00379d24 lea       rcx, [rbp - 0x70]
00379d28 call      qword ptr [rip + 0x1ecc4aa]
00379d2e mov       rdx, rax
00379d31 mov       rcx, qword ptr [rip + 0x1db98f8]
00379d38 call      0x7ff7e9f2a889 ; ?focusCamera@PlayerInterface@@QEAAXAEBVVector3@Ogre@@@Z
00379d3d mov       r11, qword ptr [rip + 0x1db98ec]
00379d44 mov       rbx, qword ptr [r11 + 0x2a0]
00379d4b movaps    xmm3, xmm7
00379d4e movaps    xmm2, xmm8
00379d52 movaps    xmm1, xmm6
00379d55 lea       rcx, [rbp - 0x70]
00379d59 call      qword ptr [rip + 0x1ecc479]
00379d5f mov       rdx, rax
00379d62 mov       rcx, rbx
00379d65 call      0x7ff7e9f20fcd ; ?resetSquadPositions@Faction@@QEAA_NAEBVVector3@Ogre@@@Z
00379d6a test      dil, 0x20
00379d6e je        0x7ff7ea289d83
00379d70 lea       rdx, [rip + 0x1db9659]
00379d77 lea       rcx, [rip + 0x1db9332]
00379d7e call      0x7ff7e9f5e57b ; ?justLoadFactionRelations@GameWorld@@QEAAXPEAVGameDataManager@@@Z
00379d83 mov       rcx, qword ptr [rip + 0x1db97ce]
00379d8a call      0x7ff7e9f61280 ; ?activateUnloadedPlatoons@FactionManager@@QEAAXXZ
00379d8f mov       r11d, dword ptr [rip + 0x1db3e7e]
00379d96 test      r11b, 1
00379d9a jne       0x7ff7ea289dce
00379d9c or        r11d, 1
00379da0 mov       dword ptr [rip + 0x1db3e6d], r11d
00379da7 mov       ecx, 0x318
00379dac call      0x7ff7eade563a
00379db1 mov       qword ptr [rbp - 0x70], rax
00379db5 test      rax, rax
00379db8 je        0x7ff7ea289dc5
00379dba mov       rcx, rax
00379dbd call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00379dc2 mov       r13, rax
00379dc5 mov       qword ptr [rip + 0x1db3e3c], r13
00379dcc jmp       0x7ff7ea289dd5
00379dce mov       r13, qword ptr [rip + 0x1db3e33]
00379dd5 mov       rcx, r13
00379dd8 call      0x7ff7e9f4378f ; ?analyse@SaveFileSystem@@QEAAXXZ
00379ddd nop
00379dde lea       rbx, [rip + 0x13046fb]
00379de5 mov       qword ptr [rsp + 0x38], rbx
00379dea mov       rcx, qword ptr [rsp + 0x48]
00379def test      rcx, rcx
00379df2 je        0x7ff7ea289df9
00379df4 call      0x7ff7eade562e
00379df9 lea       rax, [rip + 0x1304638]
00379e00 mov       qword ptr [rsp + 0x38], rax
00379e05 mov       qword ptr [rsp + 0x70], rbx
00379e0a mov       rcx, qword ptr [rbp - 0x80]
00379e0e test      rcx, rcx
00379e11 je        0x7ff7ea289e18
00379e13 call      0x7ff7eade562e
00379e18 xor       eax, eax
00379e1a mov       rcx, qword ptr [rbp + 0x310]
00379e21 xor       rcx, rsp
00379e24 call      0x7ff7eade5db0
00379e29 lea       r11, [rsp + 0x450]
00379e31 mov       rbx, qword ptr [r11 + 0x58]
00379e35 movaps    xmm6, xmmword ptr [r11 - 0x10]
00379e3a movaps    xmm7, xmmword ptr [r11 - 0x20]
00379e3f movaps    xmm8, xmmword ptr [r11 - 0x30]
00379e44 mov       rsp, r11
00379e47 pop       r15
00379e49 pop       r14
00379e4b pop       r13
00379e4d pop       r12
00379e4f pop       rdi
00379e50 pop       rsi
00379e51 pop       rbp
00379e52 ret
00379e53 int3
