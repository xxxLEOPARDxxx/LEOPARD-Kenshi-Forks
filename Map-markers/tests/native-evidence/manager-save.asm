?saveGame@SaveManager@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z RVA=375220 slot=6212 PID=9388
00375220 mov       rax, rsp
00375223 push      rbx
00375224 push      rsi
00375225 push      rdi
00375226 push      r12
00375228 push      r13
0037522a push      r14
0037522c push      r15
0037522e sub       rsp, 0x370
00375235 mov       qword ptr [rsp + 0x50], 0xfffffffffffffffe
0037523e movaps    xmmword ptr [rax - 0x48], xmm6
00375242 movaps    xmmword ptr [rax - 0x58], xmm7
00375246 movaps    xmmword ptr [rax - 0x68], xmm8
0037524b movaps    xmmword ptr [rax - 0x78], xmm11
00375250 mov       rax, qword ptr [rip + 0x1da1161]
00375257 xor       rax, rsp
0037525a mov       qword ptr [rsp + 0x320], rax
00375262 mov       r15, r8
00375265 mov       qword ptr [rsp + 0x48], rdx
0037526a mov       rdi, rcx
0037526d mov       qword ptr [rsp + 0x40], r8
00375272 xor       r12d, r12d
00375275 mov       dword ptr [rsp + 0x30], r12d
0037527a mov       r14b, 1
0037527d lea       rcx, [rip + 0x1dbe6cc]
00375284 call      0x7ff7e9f4b8f9
00375289 movapd    xmm8, xmm0
0037528e movsd     xmm7, qword ptr [rip + 0x1334a9a]
00375296 mulsd     xmm8, xmm7
0037529b lea       rcx, [rip + 0x1dbe12e]
003752a2 call      0x7ff7e9f2735a ; ?clearButDontDestroyPlatoons@GameDataContainer@@QEAAXXZ
003752a7 lea       rdx, [rip + 0x1dbe122]
003752ae mov       rcx, qword ptr [rip + 0x1dbe2a3]
003752b5 call      0x7ff7e9f15939 ; ?savePlayerGameState@FactionManager@@QEAAXPEAVGameDataContainer@@@Z
003752ba mov       rcx, qword ptr [rip + 0x1dbe69f]
003752c1 call      0x7ff7e9f3d80d ; ?saveActiveZoneStates@ZoneManager@@QEAAXXZ
003752c6 lea       rcx, [rip + 0x1dbe683]
003752cd call      0x7ff7e9f4b8f9
003752d2 movapd    xmm6, xmm0
003752d6 mulsd     xmm6, xmm7
003752da lea       rdx, [rip + 0x1dbe0ef]
003752e1 mov       rcx, qword ptr [rip + 0x1dbddb8]
003752e8 call      0x7ff7e9f5d55e ; ?saveUniqueTownsAndNests@TownList@@QEAAXPEAVGameDataContainer@@@Z
003752ed lea       rdx, [rip + 0x1dbe0dc]
003752f4 mov       rcx, qword ptr [rip + 0x1dbdda5]
003752fb call      0x7ff7e9f1e39a ; ?saveState@TownList@@QEAAXPEAVGameDataContainer@@@Z
00375300 lea       rcx, [rip + 0x1dbe649]
00375307 call      0x7ff7e9f4b8f9
0037530c mulsd     xmm0, xmm7
00375310 subsd     xmm0, xmm6
00375314 mulsd     xmm0, qword ptr [rip + 0x1342904]
0037531c addsd     xmm0, qword ptr [rip + 0x130e11c]
00375324 call      0x7ff7eade5c3c
00375329 movapd    xmm11, xmm0
0037532e mov       qword ptr [rsp + 0x1c8], 0xf
0037533a mov       qword ptr [rsp + 0x1c0], r12
00375342 mov       byte ptr [rsp + 0x1b0], r12b
0037534a lea       r8d, [r12 + 1]
0037534f lea       rdx, [rip + 0x1315f16]
00375356 lea       rcx, [rsp + 0x1b0]
0037535e call      0x7ff7e9f5d7c5
00375363 nop
00375364 mov       qword ptr [rsp + 0x1a0], 0xf
00375370 mov       qword ptr [rsp + 0x198], r12
00375378 mov       byte ptr [rsp + 0x188], r12b
00375380 xor       r8d, r8d
00375383 lea       rdx, [rip + 0x1306036]
0037538a lea       rcx, [rsp + 0x188]
00375392 call      0x7ff7e9f5d7c5
00375397 nop
00375398 lea       r9, [rsp + 0x1b0]
003753a0 lea       r8, [rsp + 0x188]
003753a8 lea       edx, [r12 + 0x38]
003753ad lea       rcx, [rip + 0x1dbe01c]
003753b4 call      0x7ff7e9f272ba ; ?createNewData@GameDataContainer@@QEAAPEAVGameData@@W4itemType@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1@Z
003753b9 mov       r13, rax
003753bc cmp       qword ptr [rsp + 0x1a0], 0x10
003753c5 jb        0x7ff7ea2853d4
003753c7 mov       rcx, qword ptr [rsp + 0x188]
003753cf call      0x7ff7eade5628
003753d4 mov       qword ptr [rsp + 0x1a0], 0xf
003753e0 mov       qword ptr [rsp + 0x198], r12
003753e8 mov       byte ptr [rsp + 0x188], 0
003753f0 cmp       qword ptr [rsp + 0x1c8], 0x10
003753f9 jb        0x7ff7ea285408
003753fb mov       rcx, qword ptr [rsp + 0x1b0]
00375403 call      0x7ff7eade5628
00375408 mov       qword ptr [rsp + 0x1c8], 0xf
00375414 mov       qword ptr [rsp + 0x1c0], r12
0037541c mov       byte ptr [rsp + 0x1b0], 0
00375424 mov       rdx, r13
00375427 mov       rcx, qword ptr [rip + 0x1dbe202]
0037542e mov       rcx, qword ptr [rcx + 0x30]
00375432 call      0x7ff7e9f25e6f ; ?save@CameraClass@@QEAAXPEAVGameData@@@Z
00375437 mov       qword ptr [rsp + 0x98], 0xf
00375443 mov       qword ptr [rsp + 0x90], r12
0037544b mov       byte ptr [rsp + 0x80], 0
00375453 mov       r8d, 0xc
00375459 lea       rdx, [rip + 0x134e5d0]
00375460 lea       rcx, [rsp + 0x80]
00375468 call      0x7ff7e9f5d7c5
0037546d nop
0037546e mov       rax, qword ptr [rip + 0x1dbe1bb]
00375475 mov       rcx, qword ptr [rax + 0x2a0]
0037547c mov       rax, qword ptr [rcx + 0x80]
00375483 mov       ebx, dword ptr [rax + 0x88]
00375489 lea       rdx, [rsp + 0x80]
00375491 lea       rcx, [r13 + 0x178]
00375498 call      0x7ff7e9f580fe
0037549d mov       dword ptr [rax + 0x28], ebx
003754a0 cmp       qword ptr [rsp + 0x98], 0x10
003754a9 jb        0x7ff7ea2854b8
003754ab mov       rcx, qword ptr [rsp + 0x80]
003754b3 call      0x7ff7eade5628
003754b8 mov       qword ptr [rsp + 0x98], 0xf
003754c4 mov       qword ptr [rsp + 0x90], r12
003754cc mov       byte ptr [rsp + 0x80], 0
003754d4 mov       r8d, 0xd
003754da lea       rdx, [rip + 0x134e56f]
003754e1 lea       rcx, [rsp + 0x80]
003754e9 call      0x7ff7e9f5d7c5
003754ee nop
003754ef lea       rdx, [rsp + 0x80]
003754f7 lea       rcx, [r13 + 0x138]
003754fe call      0x7ff7e9f11aeb
00375503 lea       rcx, [rax + 0x28]
00375507 mov       rdx, qword ptr [rip + 0x1dbe122]
0037550e add       rdx, 8
00375512 or        r9, 0xffffffffffffffff
00375516 xor       r8d, r8d
00375519 call      0x7ff7e9f3808d
0037551e nop
0037551f cmp       qword ptr [rsp + 0x98], 0x10
00375528 jb        0x7ff7ea285537
0037552a mov       rcx, qword ptr [rsp + 0x80]
00375532 call      0x7ff7eade5628
00375537 mov       qword ptr [rsp + 0x98], 0xf
00375543 xor       ebx, ebx
00375545 mov       qword ptr [rsp + 0x90], rbx
0037554d mov       byte ptr [rsp + 0x80], bl
00375554 lea       r8d, [rbx + 7]
00375558 lea       rdx, [rip + 0x1334089]
0037555f lea       rcx, [rsp + 0x80]
00375567 call      0x7ff7e9f5d7c5
0037556c nop
0037556d lea       rdx, [rsp + 0x80]
00375575 lea       rcx, [r13 + 0x138]
0037557c call      0x7ff7e9f11aeb
00375581 lea       rcx, [rax + 0x28]
00375585 lea       rdx, [rdi + 0x28]
00375589 or        r9, 0xffffffffffffffff
0037558d xor       r8d, r8d
00375590 call      0x7ff7e9f3808d
00375595 nop
00375596 cmp       qword ptr [rsp + 0x98], 0x10
0037559f jb        0x7ff7ea2855ae
003755a1 mov       rcx, qword ptr [rsp + 0x80]
003755a9 call      0x7ff7eade5628
003755ae mov       qword ptr [rsp + 0x98], 0xf
003755ba mov       qword ptr [rsp + 0x90], rbx
003755c2 mov       byte ptr [rsp + 0x80], 0
003755ca mov       r8d, 6
003755d0 lea       rdx, [rip + 0x1341e0d]
003755d7 lea       rcx, [rsp + 0x80]
003755df call      0x7ff7e9f5d7c5
003755e4 nop
003755e5 mov       rax, qword ptr [rip + 0x1dbe044]
003755ec mov       rcx, qword ptr [rax + 0x2a0]
003755f3 mov       ebx, dword ptr [rcx + 0x210]
003755f9 lea       rdx, [rsp + 0x80]
00375601 lea       rcx, [r13 + 0x178]
00375608 call      0x7ff7e9f580fe
0037560d mov       dword ptr [rax + 0x28], ebx
00375610 cmp       qword ptr [rsp + 0x98], 0x10
00375619 jb        0x7ff7ea285628
0037561b mov       rcx, qword ptr [rsp + 0x80]
00375623 call      0x7ff7eade5628
00375628 mov       qword ptr [rsp + 0x98], 0xf
00375634 mov       qword ptr [rsp + 0x90], 0
00375640 mov       byte ptr [rsp + 0x80], 0
00375648 mov       r8d, 7
0037564e lea       rdx, [rip + 0x134e57b]
00375655 lea       rcx, [rsp + 0x80]
0037565d call      0x7ff7e9f5d7c5
00375662 nop
00375663 mov       rax, qword ptr [rip + 0x1dbdfc6]
0037566a mov       rdi, qword ptr [rax + 0x2a0]
00375671 lea       rdx, [rsp + 0x80]
00375679 lea       rcx, [r13 + 0x178]
00375680 call      0x7ff7e9f580fe
00375685 mov       rbx, rax
00375688 mov       rcx, rdi
0037568b call      0x7ff7e9f28c32 ; ?getFactionSize@Faction@@QEBAHXZ
00375690 mov       dword ptr [rbx + 0x28], eax
00375693 cmp       qword ptr [rsp + 0x98], 0x10
0037569c jb        0x7ff7ea2856ab
0037569e mov       rcx, qword ptr [rsp + 0x80]
003756a6 call      0x7ff7eade5628
003756ab mov       qword ptr [rsp + 0x98], 0xf
003756b7 mov       qword ptr [rsp + 0x90], 0
003756c3 mov       byte ptr [rsp + 0x80], 0
003756cb mov       r8d, 9
003756d1 lea       rdx, [rip + 0x134e348]
003756d8 lea       rcx, [rsp + 0x80]
003756e0 call      0x7ff7e9f5d7c5
003756e5 nop
003756e6 mov       rax, qword ptr [rip + 0x1dbdf43]
003756ed mov       ebx, dword ptr [rax + 0x1e4]
003756f3 lea       rdx, [rsp + 0x80]
003756fb lea       rcx, [r13 + 0x178]
00375702 call      0x7ff7e9f580fe
00375707 mov       dword ptr [rax + 0x28], ebx
0037570a cmp       qword ptr [rsp + 0x98], 0x10
00375713 jb        0x7ff7ea285722
00375715 mov       rcx, qword ptr [rsp + 0x80]
0037571d call      0x7ff7eade5628
00375722 lea       rdx, [rsp + 0x70]
00375727 lea       rcx, [rip + 0x1dbd982]
0037572e call      0x7ff7e9f62b8a ; ?getCameraCenter@GameWorld@@QEBA?BVVector3@Ogre@@XZ
00375733 mov       rdx, rax
00375736 mov       rcx, qword ptr [rip + 0x1dbd95b]
0037573d call      0x7ff7e9f26c84
00375742 mov       rdx, qword ptr [rax + 0x10]
00375746 add       rdx, 0x28
0037574a mov       qword ptr [rsp + 0x1f0], 0xf
00375756 xor       esi, esi
00375758 mov       qword ptr [rsp + 0x1e8], rsi
00375760 mov       byte ptr [rsp + 0x1d8], sil
00375768 or        r9, 0xffffffffffffffff
0037576c xor       r8d, r8d
0037576f lea       rcx, [rsp + 0x1d8]
00375777 call      0x7ff7e9f3808d
0037577c nop
0037577d mov       rbx, qword ptr [rsp + 0x1e8]
00375785 test      rbx, rbx
00375788 je        0x7ff7ea2858ea
0037578e lea       rcx, [rsp + 0x1d8]
00375796 mov       rdi, qword ptr [rsp + 0x1d8]
0037579e mov       rsi, qword ptr [rsp + 0x1f0]
003757a6 cmp       rsi, 0x10
003757aa cmovae    rcx, rdi
003757ae mov       r8d, 4
003757b4 cmp       rbx, r8
003757b7 cmovb     r8, rbx
003757bb lea       rdx, [rip + 0x134e402]
003757c2 call      0x7ff7eade5c12
003757c7 movsxd    rcx, eax
003757ca test      eax, eax
003757cc jne       0x7ff7ea2857e7
003757ce cmp       rbx, 4
003757d2 jae       0x7ff7ea2857dc
003757d4 or        rdx, 0xffffffffffffffff
003757d8 mov       ecx, edx
003757da jmp       0x7ff7ea2857eb
003757dc xor       eax, eax
003757de cmp       rbx, 4
003757e2 setne     al
003757e5 mov       ecx, eax
003757e7 or        rdx, 0xffffffffffffffff
003757eb test      ecx, ecx
003757ed sete      al
003757f0 test      al, al
003757f2 jne       0x7ff7ea2858e8
003757f8 mov       r8d, 0xf
003757fe mov       qword ptr [rsp + 0x110], r8
00375806 mov       qword ptr [rsp + 0x108], 0
00375812 mov       byte ptr [rsp + 0xf8], al
00375819 cmp       rbx, -1
0037581d cmova     rbx, rdx
00375821 cmp       rbx, -2
00375825 jbe       0x7ff7ea285834
00375827 lea       rcx, [rip + 0x130540a]
0037582e call      qword ptr [rip + 0x1ecedbc]
00375834 cmp       rbx, r8
00375837 jbe       0x7ff7ea285866
00375839 xor       r8d, r8d
0037583c mov       rdx, rbx
0037583f lea       rcx, [rsp + 0xf8]
00375847 call      0x7ff7e9f3010d
0037584c mov       rsi, qword ptr [rsp + 0x1f0]
00375854 mov       rdi, qword ptr [rsp + 0x1d8]
0037585c mov       r8, qword ptr [rsp + 0x110]
00375864 jmp       0x7ff7ea28587a
00375866 test      rbx, rbx
00375869 jne       0x7ff7ea28587d
0037586b mov       qword ptr [rsp + 0x108], rbx
00375873 mov       byte ptr [rsp + 0xf8], bl
0037587a test      rbx, rbx
0037587d setne     al
00375880 test      al, al
00375882 je        0x7ff7ea2858d7
00375884 lea       rdx, [rsp + 0x1d8]
0037588c cmp       rsi, 0x10
00375890 cmovae    rdx, rdi
00375894 lea       rcx, [rsp + 0xf8]
0037589c cmp       r8, 0x10
003758a0 cmovae    rcx, qword ptr [rsp + 0xf8]
003758a9 mov       r8, rbx
003758ac call      0x7ff7eade5c18
003758b1 mov       qword ptr [rsp + 0x108], rbx
003758b9 lea       rax, [rsp + 0xf8]
003758c1 cmp       qword ptr [rsp + 0x110], 0x10
003758ca cmovae    rax, qword ptr [rsp + 0xf8]
003758d3 mov       byte ptr [rax + rbx], 0
003758d7 lea       rdi, [rsp + 0xf8]
003758df mov       ebx, 1
003758e4 xor       esi, esi
003758e6 jmp       0x7ff7ea28592d
003758e8 xor       esi, esi
003758ea mov       qword ptr [rsp + 0x98], 0xf
003758f6 mov       qword ptr [rsp + 0x90], rsi
003758fe mov       byte ptr [rsp + 0x80], 0
00375906 mov       r8d, 1
0037590c lea       rdx, [rip + 0x1312709]
00375913 lea       rcx, [rsp + 0x80]
0037591b call      0x7ff7e9f5d7c5
00375920 lea       rdi, [rsp + 0x80]
00375928 mov       ebx, 2
0037592d mov       dword ptr [rsp + 0x30], ebx
00375931 mov       qword ptr [rsp + 0xc0], 0xf
0037593d mov       qword ptr [rsp + 0xb8], rsi
00375945 mov       byte ptr [rsp + 0xa8], 0
0037594d mov       r8d, 4
00375953 lea       rdx, [rip + 0x134e262]
0037595a lea       rcx, [rsp + 0xa8]
00375962 call      0x7ff7e9f5d7c5
00375967 nop
00375968 lea       rdx, [rsp + 0xa8]
00375970 lea       rcx, [r13 + 0x138]
00375977 call      0x7ff7e9f11aeb
0037597c lea       rcx, [rax + 0x28]
00375980 mov       rdx, rdi
00375983 call      0x7ff7e9f121a3
00375988 nop
00375989 cmp       qword ptr [rsp + 0xc0], 0x10
00375992 jb        0x7ff7ea2859a1
00375994 mov       rcx, qword ptr [rsp + 0xa8]
0037599c call      0x7ff7eade5628
003759a1 mov       qword ptr [rsp + 0xc0], 0xf
003759ad mov       qword ptr [rsp + 0xb8], rsi
003759b5 mov       byte ptr [rsp + 0xa8], 0
003759bd test      bl, 2
003759c0 je        0x7ff7ea2859f9
003759c2 and       ebx, 0xfffffffd
003759c5 cmp       qword ptr [rsp + 0x98], 0x10
003759ce jb        0x7ff7ea2859dd
003759d0 mov       rcx, qword ptr [rsp + 0x80]
003759d8 call      0x7ff7eade5628
003759dd mov       qword ptr [rsp + 0x98], 0xf
003759e9 mov       qword ptr [rsp + 0x90], rsi
003759f1 mov       byte ptr [rsp + 0x80], 0
003759f9 test      bl, 1
003759fc je        0x7ff7ea285a16
003759fe cmp       qword ptr [rsp + 0x110], 0x10
00375a07 jb        0x7ff7ea285a16
00375a09 mov       rcx, qword ptr [rsp + 0xf8]
00375a11 call      0x7ff7eade5628
00375a16 mov       rdx, r13
00375a19 mov       rcx, qword ptr [rip + 0x1dbdc10]
00375a20 call      0x7ff7e9f551b5 ; ?serialise@PlayerInterface@@QEAAXPEAVGameData@@@Z
00375a25 mov       rdx, r13
00375a28 mov       rcx, qword ptr [rip + 0x1dbcd31]
00375a2f call      0x7ff7e9f2806b ; ?serialise@MainBarGUI@@QEAAXPEAVGameData@@@Z
00375a34 mov       rax, qword ptr [rip + 0x1dbc87d]
00375a3b mov       rcx, qword ptr [rax + 0x20]
00375a3f test      rcx, rcx
00375a42 je        0x7ff7ea285a49
00375a44 call      0x7ff7e9f31940
00375a49 mov       rcx, r13
00375a4c call      0x7ff7e9f1a51a ; ?save@DialogDataManager@@SAXPEAVGameData@@@Z
00375a51 mov       rdx, r13
00375a54 lea       rcx, [rip + 0x1dbcacd]
00375a5b call      0x7ff7e9f41886 ; ?save@GameplayOptions@@QEAAXPEAVGameData@@@Z
00375a60 mov       rcx, qword ptr [rip + 0x1dbdbc9]
00375a67 add       rcx, 0x110
00375a6e mov       rdx, r13
00375a71 call      0x7ff7e9f5b97a ; ?save@AIOptions@PlayerInterface@@QEAAXPEAVGameData@@@Z
00375a76 mov       rdx, r13
00375a79 mov       rcx, qword ptr [rip + 0x1db9940]
00375a80 call      0x7ff7e9f4f6d9
00375a85 call      0x7ff7e9f2c841
00375a8a mov       rdx, r13
00375a8d mov       rcx, rax
00375a90 call      0x7ff7e9f24849
00375a95 mov       qword ptr [rsp + 0xc0], 0xf
00375aa1 mov       qword ptr [rsp + 0xb8], rsi
00375aa9 mov       byte ptr [rsp + 0xa8], 0
00375ab1 mov       r8d, 1
00375ab7 lea       rdx, [rip + 0x13157ae]
00375abe lea       rcx, [rsp + 0xa8]
00375ac6 call      0x7ff7e9f5d7c5
00375acb nop
00375acc mov       qword ptr [rsp + 0x98], 0xf
00375ad8 mov       qword ptr [rsp + 0x90], rsi
00375ae0 mov       byte ptr [rsp + 0x80], 0
00375ae8 xor       r8d, r8d
00375aeb lea       rdx, [rip + 0x13058ce]
00375af2 lea       rcx, [rsp + 0x80]
00375afa call      0x7ff7e9f5d7c5
00375aff nop
00375b00 lea       r9, [rsp + 0xa8]
00375b08 lea       r8, [rsp + 0x80]
00375b10 mov       edx, 0x15
00375b15 lea       rcx, [rip + 0x1dbd8b4]
00375b1c call      0x7ff7e9f272ba ; ?createNewData@GameDataContainer@@QEAAPEAVGameData@@W4itemType@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1@Z
00375b21 mov       rbx, rax
00375b24 cmp       qword ptr [rsp + 0x98], 0x10
00375b2d jb        0x7ff7ea285b3c
00375b2f mov       rcx, qword ptr [rsp + 0x80]
00375b37 call      0x7ff7eade5628
00375b3c mov       qword ptr [rsp + 0x98], 0xf
00375b48 mov       qword ptr [rsp + 0x90], rsi
00375b50 mov       byte ptr [rsp + 0x80], 0
00375b58 cmp       qword ptr [rsp + 0xc0], 0x10
00375b61 jb        0x7ff7ea285b70
00375b63 mov       rcx, qword ptr [rsp + 0xa8]
00375b6b call      0x7ff7eade5628
00375b70 mov       qword ptr [rsp + 0xc0], 0xf
00375b7c mov       qword ptr [rsp + 0xb8], rsi
00375b84 mov       byte ptr [rsp + 0xa8], 0
00375b8c mov       rdx, rbx
00375b8f mov       rcx, qword ptr [rip + 0x1dbda9a]
00375b96 mov       rcx, qword ptr [rcx + 0x38]
00375b9a call      0x7ff7e9f347c6 ; ?serialise@Research@@QEAAXPEAVGameData@@@Z
00375b9f mov       qword ptr [rsp + 0xc0], 0xf
00375bab mov       qword ptr [rsp + 0xb8], rsi
00375bb3 mov       byte ptr [rsp + 0xa8], 0
00375bbb mov       r8d, 1
00375bc1 lea       rdx, [rip + 0x13156a4]
00375bc8 lea       rcx, [rsp + 0xa8]
00375bd0 call      0x7ff7e9f5d7c5
00375bd5 nop
00375bd6 mov       qword ptr [rsp + 0x98], 0xf
00375be2 mov       qword ptr [rsp + 0x90], rsi
00375bea mov       byte ptr [rsp + 0x80], 0
00375bf2 xor       r8d, r8d
00375bf5 lea       rdx, [rip + 0x13057c4]
00375bfc lea       rcx, [rsp + 0x80]
00375c04 call      0x7ff7e9f5d7c5
00375c09 nop
00375c0a lea       r9, [rsp + 0xa8]
00375c12 lea       r8, [rsp + 0x80]
00375c1a mov       edx, 0x1c
00375c1f lea       rcx, [rip + 0x1dbd7aa]
00375c26 call      0x7ff7e9f272ba ; ?createNewData@GameDataContainer@@QEAAPEAVGameData@@W4itemType@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1@Z
00375c2b mov       rbx, rax
00375c2e cmp       qword ptr [rsp + 0x98], 0x10
00375c37 jb        0x7ff7ea285c46
00375c39 mov       rcx, qword ptr [rsp + 0x80]
00375c41 call      0x7ff7eade5628
00375c46 mov       qword ptr [rsp + 0x98], 0xf
00375c52 mov       qword ptr [rsp + 0x90], rsi
00375c5a mov       byte ptr [rsp + 0x80], 0
00375c62 cmp       qword ptr [rsp + 0xc0], 0x10
00375c6b jb        0x7ff7ea285c7a
00375c6d mov       rcx, qword ptr [rsp + 0xa8]
00375c75 call      0x7ff7eade5628
00375c7a mov       qword ptr [rsp + 0xc0], 0xf
00375c86 mov       qword ptr [rsp + 0xb8], rsi
00375c8e mov       byte ptr [rsp + 0xa8], 0
00375c96 mov       rdx, rbx
00375c99 mov       rcx, qword ptr [rip + 0x1dbd3f8]
00375ca0 call      0x7ff7e9f56146
00375ca5 cmp       qword ptr [rip + 0x1dbd3cb], 0
00375cad je        0x7ff7ea285f3c
00375cb3 mov       qword ptr [rsp + 0xc0], 0xf
00375cbf mov       qword ptr [rsp + 0xb8], rsi
00375cc7 mov       byte ptr [rsp + 0xa8], 0
00375ccf mov       r8d, 1
00375cd5 lea       rdx, [rip + 0x1315590]
00375cdc lea       rcx, [rsp + 0xa8]
00375ce4 call      0x7ff7e9f5d7c5
00375ce9 nop
00375cea mov       qword ptr [rsp + 0x98], 0xf
00375cf6 mov       qword ptr [rsp + 0x90], rsi
00375cfe mov       byte ptr [rsp + 0x80], 0
00375d06 mov       r8d, 0xa
00375d0c lea       rdx, [rip + 0x134dd2d]
00375d13 lea       rcx, [rsp + 0x80]
00375d1b call      0x7ff7e9f5d7c5
00375d20 nop
00375d21 lea       r9, [rsp + 0xa8]
00375d29 lea       r8, [rsp + 0x80]
00375d31 mov       edx, 0x28
00375d36 lea       rcx, [rip + 0x1dbd693]
00375d3d call      0x7ff7e9f272ba ; ?createNewData@GameDataContainer@@QEAAPEAVGameData@@W4itemType@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@1@Z
00375d42 mov       rsi, rax
00375d45 cmp       qword ptr [rsp + 0x98], 0x10
00375d4e jb        0x7ff7ea285d5d
00375d50 mov       rcx, qword ptr [rsp + 0x80]
00375d58 call      0x7ff7eade5628
00375d5d mov       qword ptr [rsp + 0x98], 0xf
00375d69 xor       r12d, r12d
00375d6c mov       qword ptr [rsp + 0x90], r12
00375d74 mov       byte ptr [rsp + 0x80], r12b
00375d7c cmp       qword ptr [rsp + 0xc0], 0x10
00375d85 jb        0x7ff7ea285d94
00375d87 mov       rcx, qword ptr [rsp + 0xa8]
00375d8f call      0x7ff7eade5628
00375d94 mov       qword ptr [rsp + 0xc0], 0xf
00375da0 mov       qword ptr [rsp + 0xb8], r12
00375da8 mov       byte ptr [rsp + 0xa8], 0
00375db0 mov       edi, r12d
00375db3 mov       qword ptr [rsp + 0x110], 0xf
00375dbf mov       qword ptr [rsp + 0x108], r12
00375dc7 mov       byte ptr [rsp + 0xf8], dil
00375dcf mov       r8d, 5
00375dd5 lea       rdx, [rip + 0x13284c8]
00375ddc lea       rcx, [rsp + 0xf8]
00375de4 call      0x7ff7e9f5d7c5
00375de9 nop
00375dea mov       rbx, qword ptr [rip + 0x1dbd287]
00375df1 lea       rcx, [rsi + 0x178]
00375df8 lea       rdx, [rsp + 0xf8]
00375e00 call      0x7ff7e9f580fe
00375e05 mov       dword ptr [rax + 0x28], ebx
00375e08 cmp       qword ptr [rsp + 0x110], 0x10
00375e11 jb        0x7ff7ea285e20
00375e13 mov       rcx, qword ptr [rsp + 0xf8]
00375e1b call      0x7ff7eade5628
00375e20 lea       rbx, [rsp + 0x30]
00375e25 cmp       qword ptr [rip + 0x1dbd24b], 0
00375e2d je        0x7ff7ea285e48
00375e2f mov       rcx, qword ptr [rip + 0x1dbd25a]
00375e36 mov       rax, qword ptr [rip + 0x1dbd233]
00375e3d mov       rax, qword ptr [rcx + rax*8]
00375e41 mov       qword ptr [rsp + 0x30], rax
00375e46 jmp       0x7ff7ea285e4d
00375e48 mov       qword ptr [rsp + 0x30], r12
00375e4d mov       rbx, qword ptr [rbx]
00375e50 test      rbx, rbx
00375e53 je        0x7ff7ea285f3a
00375e59 xor       r8d, r8d
00375e5c mov       dword ptr [rsp + 0x20], r12d
00375e61 mov       r9b, 0x20
00375e64 mov       edx, edi
00375e66 lea       rcx, [rsp + 0xd0]
00375e6e call      qword ptr [rip + 0x1ed026c]
00375e74 nop
00375e75 lea       rdx, [rbx + 0x10]
00375e79 xor       r9d, r9d
00375e7c mov       r8, rax
00375e7f mov       rcx, rsi
00375e82 call      0x7ff7e9f18431 ; ?storeHandle@GameData@@QEAAXAEBVhand@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z
00375e87 nop
00375e88 cmp       qword ptr [rsp + 0xe8], 0x10
00375e91 jb        0x7ff7ea285ea0
00375e93 mov       rcx, qword ptr [rsp + 0xd0]
00375e9b call      0x7ff7eade5628
00375ea0 xor       r8d, r8d
00375ea3 mov       dword ptr [rsp + 0x20], r12d
00375ea8 mov       r9b, 0x20
00375eab mov       edx, edi
00375ead lea       rcx, [rsp + 0xd0]
00375eb5 call      qword ptr [rip + 0x1ed0225]
00375ebb nop
00375ebc mov       r8, rax
00375ebf lea       rdx, [rip + 0x134b272]
00375ec6 lea       rcx, [rsp + 0xf8]
00375ece call      0x7ff7e9f623dd
00375ed3 nop
00375ed4 lea       rdx, [rbx + 0x30]
00375ed8 xor       r9d, r9d
00375edb mov       r8, rax
00375ede mov       rcx, rsi
00375ee1 call      0x7ff7e9f18431 ; ?storeHandle@GameData@@QEAAXAEBVhand@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z
00375ee6 nop
00375ee7 cmp       qword ptr [rsp + 0x110], 0x10
00375ef0 jb        0x7ff7ea285eff
00375ef2 mov       rcx, qword ptr [rsp + 0xf8]
00375efa call      0x7ff7eade5628
00375eff mov       qword ptr [rsp + 0x110], 0xf
00375f0b mov       qword ptr [rsp + 0x108], r12
00375f13 mov       byte ptr [rsp + 0xf8], 0
00375f1b cmp       qword ptr [rsp + 0xe8], 0x10
00375f24 jb        0x7ff7ea285f33
00375f26 mov       rcx, qword ptr [rsp + 0xd0]
00375f2e call      0x7ff7eade5628
00375f33 inc       edi
00375f35 jmp       0x7ff7ea285e4d
00375f3a xor       esi, esi
00375f3c mov       qword ptr [rsp + 0x98], 0xf
00375f48 mov       qword ptr [rsp + 0x90], rsi
00375f50 mov       byte ptr [rsp + 0x80], 0
00375f58 mov       r8d, 4
00375f5e lea       rdx, [rip + 0x134dc4f]
00375f65 lea       rcx, [rsp + 0x80]
00375f6d call      0x7ff7e9f5d7c5
00375f72 nop
00375f73 lea       rcx, [r13 + 0x2b8]
00375f7a lea       rdx, [rsp + 0x80]
00375f82 call      0x7ff7e9f14dea
00375f87 lea       rbx, [rax + 0x28]
00375f8b cmp       qword ptr [rsp + 0x98], 0x10
00375f94 jb        0x7ff7ea285fa3
00375f96 mov       rcx, qword ptr [rsp + 0x80]
00375f9e call      0x7ff7eade5628
00375fa3 mov       qword ptr [rsp + 0x98], 0xf
00375faf mov       qword ptr [rsp + 0x90], rsi
00375fb7 mov       byte ptr [rsp + 0x80], 0
00375fbf mov       r12d, esi
00375fc2 cmp       r12d, dword ptr [rip + 0x1dbd617]
00375fc9 jae       0x7ff7ea286175
00375fcf movsxd    r8, r12d
00375fd2 mov       rdx, qword ptr [rip + 0x1dbd60f]
00375fd9 mov       rax, qword ptr [rdx + r8*8]
00375fdd mov       ecx, dword ptr [rax + 0xf8]
00375fe3 mov       rdx, rax
00375fe6 mov       dword ptr [rsp + 0x120], ecx
00375fed mov       qword ptr [rsp + 0x124], 0
00375ff9 mov       qword ptr [rsp + 0x148], 0xf
00376005 mov       qword ptr [rsp + 0x140], rsi
0037600d mov       byte ptr [rsp + 0x130], 0
00376015 or        r9, 0xffffffffffffffff
00376019 xor       r8d, r8d
0037601c lea       rcx, [rsp + 0x130]
00376024 call      0x7ff7e9f3808d
00376029 mov       qword ptr [rsp + 0x158], rsi
00376031 mov       rax, qword ptr [rbx + 8]
00376035 lea       rcx, [rsp + 0x120]
0037603d cmp       rcx, rax
00376040 jae       0x7ff7ea2860da
00376046 mov       rcx, qword ptr [rbx]
00376049 lea       rdx, [rsp + 0x120]
00376051 cmp       rcx, rdx
00376054 ja        0x7ff7ea2860da
0037605a lea       rdi, [rsp + 0x120]
00376062 sub       rdi, rcx
00376065 cmp       rax, qword ptr [rbx + 0x10]
00376069 jne       0x7ff7ea286078
0037606b mov       edx, 1
00376070 mov       rcx, rbx
00376073 call      0x7ff7e9f622b6
00376078 and       rdi, 0xffffffffffffffc0
0037607c add       rdi, qword ptr [rbx]
0037607f mov       rsi, qword ptr [rbx + 8]
00376083 mov       qword ptr [rsp + 0x30], rsi
00376088 mov       qword ptr [rsp + 0x38], rsi
0037608d test      rsi, rsi
00376090 je        0x7ff7ea2860d1
00376092 mov       eax, dword ptr [rdi]
00376094 mov       dword ptr [rsi], eax
00376096 mov       eax, dword ptr [rdi + 4]
00376099 mov       dword ptr [rsi + 4], eax
0037609c mov       eax, dword ptr [rdi + 8]
0037609f mov       dword ptr [rsi + 8], eax
003760a2 lea       rcx, [rsi + 0x10]
003760a6 mov       qword ptr [rcx + 0x18], 0xf
003760ae mov       qword ptr [rcx + 0x10], 0
003760b6 mov       byte ptr [rcx], 0
003760b9 lea       rdx, [rdi + 0x10]
003760bd or        r9, 0xffffffffffffffff
003760c1 xor       r8d, r8d
003760c4 call      0x7ff7e9f3808d
003760c9 mov       rax, qword ptr [rdi + 0x38]
003760cd mov       qword ptr [rsi + 0x38], rax
003760d1 add       qword ptr [rbx + 8], 0x40
003760d6 xor       esi, esi
003760d8 jmp       0x7ff7ea286155
003760da cmp       rax, qword ptr [rbx + 0x10]
003760de jne       0x7ff7ea2860ed
003760e0 mov       edx, 1
003760e5 mov       rcx, rbx
003760e8 call      0x7ff7e9f622b6
003760ed mov       rdi, qword ptr [rbx + 8]
003760f1 mov       qword ptr [rsp + 0x38], rdi
003760f6 mov       qword ptr [rsp + 0x30], rdi
003760fb test      rdi, rdi
003760fe je        0x7ff7ea286150
00376100 mov       eax, dword ptr [rsp + 0x120]
00376107 mov       dword ptr [rdi], eax
00376109 mov       eax, dword ptr [rsp + 0x124]
00376110 mov       dword ptr [rdi + 4], eax
00376113 mov       eax, dword ptr [rsp + 0x128]
0037611a mov       dword ptr [rdi + 8], eax
0037611d lea       rcx, [rdi + 0x10]
00376121 mov       qword ptr [rcx + 0x18], 0xf
00376129 mov       qword ptr [rcx + 0x10], rsi
0037612d mov       byte ptr [rcx], 0
00376130 or        r9, 0xffffffffffffffff
00376134 xor       r8d, r8d
00376137 lea       rdx, [rsp + 0x130]
0037613f call      0x7ff7e9f3808d
00376144 mov       rax, qword ptr [rsp + 0x158]
0037614c mov       qword ptr [rdi + 0x38], rax
00376150 add       qword ptr [rbx + 8], 0x40
00376155 cmp       qword ptr [rsp + 0x148], 0x10
0037615e jb        0x7ff7ea28616d
00376160 mov       rcx, qword ptr [rsp + 0x130]
00376168 call      0x7ff7eade5628
0037616d inc       r12d
00376170 jmp       0x7ff7ea285fc2
00376175 mov       rcx, qword ptr [rip + 0x1dbd7e4]
0037617c call      0x7ff7e9f4e18a ; ?checkZoneFiles@ZoneManager@@QEBA_NXZ
00376181 mov       qword ptr [rsp + 0x98], 0xf
0037618d xor       r12d, r12d
00376190 mov       qword ptr [rsp + 0x90], r12
00376198 mov       byte ptr [rsp + 0x80], r12b
003761a0 lea       r8d, [r12 + 5]
003761a5 lea       rdx, [rip + 0x134d86c]
003761ac lea       rcx, [rsp + 0x80]
003761b4 call      0x7ff7e9f5d7c5
003761b9 nop
003761ba lea       rcx, [r13 + 0x2b8]
003761c1 lea       rdx, [rsp + 0x80]
003761c9 call      0x7ff7e9f14dea
003761ce lea       rsi, [rax + 0x28]
003761d2 cmp       qword ptr [rsp + 0x98], 0x10
003761db jb        0x7ff7ea2861ea
003761dd mov       rcx, qword ptr [rsp + 0x80]
003761e5 call      0x7ff7eade5628
003761ea mov       qword ptr [rsp + 0x98], 0xf
003761f6 mov       qword ptr [rsp + 0x90], r12
003761fe mov       byte ptr [rsp + 0x80], 0
00376206 mov       edi, r12d
00376209 nop       dword ptr [rax]
00376210 cmp       edi, 0x40
00376213 jge       0x7ff7ea28631f
00376219 mov       ebx, r12d
0037621c nop       dword ptr [rax]
00376220 cmp       ebx, 0x40
00376223 jge       0x7ff7ea286318
00376229 mov       r8d, ebx
0037622c mov       edx, edi
0037622e mov       rcx, qword ptr [rip + 0x1dbd72b]
00376235 call      0x7ff7e9f27bac ; ?getZoneMap@ZoneManager@@QEAAPEAVZoneMap@@HH@Z
0037623a cmp       byte ptr [rax + 0x24], 0
0037623e je        0x7ff7ea286311
00376244 mov       rdx, qword ptr [rsi + 8]
00376248 sub       rdx, qword ptr [rsi]
0037624b sar       rdx, 6
0037624f xor       r8d, r8d
00376252 lea       rcx, [rsp + 0xd0]
0037625a call      0x7ff7e9f2a3ed
0037625f nop
00376260 mov       dword ptr [rsp + 0x120], edi
00376267 mov       dword ptr [rsp + 0x124], ebx
0037626e mov       dword ptr [rsp + 0x128], 1
00376279 mov       qword ptr [rsp + 0x148], 0xf
00376285 mov       qword ptr [rsp + 0x140], r12
0037628d mov       byte ptr [rsp + 0x130], 0
00376295 or        r9, 0xffffffffffffffff
00376299 xor       r8d, r8d
0037629c mov       rdx, rax
0037629f lea       rcx, [rsp + 0x130]
003762a7 call      0x7ff7e9f3808d
003762ac mov       qword ptr [rsp + 0x158], r12
003762b4 lea       rdx, [rsp + 0x120]
003762bc mov       rcx, rsi
003762bf call      0x7ff7e9f3b67f
003762c4 nop
003762c5 cmp       qword ptr [rsp + 0x148], 0x10
003762ce jb        0x7ff7ea2862dd
003762d0 mov       rcx, qword ptr [rsp + 0x130]
003762d8 call      0x7ff7eade5628
003762dd mov       qword ptr [rsp + 0x148], 0xf
003762e9 mov       qword ptr [rsp + 0x140], r12
003762f1 mov       byte ptr [rsp + 0x130], 0
003762f9 cmp       qword ptr [rsp + 0xe8], 0x10
00376302 jb        0x7ff7ea286311
00376304 mov       rcx, qword ptr [rsp + 0xd0]
0037630c call      0x7ff7eade5628
00376311 inc       ebx
00376313 jmp       0x7ff7ea286220
00376318 inc       edi
0037631a jmp       0x7ff7ea286210
0037631f lea       rcx, [rip + 0x1dbd62a]
00376326 call      0x7ff7e9f4b8f9
0037632b movapd    xmm6, xmm0
0037632f mulsd     xmm6, xmm7
00376333 lea       rdx, [rip + 0x1dbd096]
0037633a mov       rcx, qword ptr [rip + 0x1dbd217]
00376341 call      0x7ff7e9f60f4c ; ?saveGameState@FactionManager@@QEAAXPEAVGameDataContainer@@@Z
00376346 lea       rcx, [rip + 0x1dbd603]
0037634d call      0x7ff7e9f4b8f9
00376352 mulsd     xmm0, xmm7
00376356 subsd     xmm0, xmm6
0037635a mulsd     xmm0, qword ptr [rip + 0x13418be]
00376362 addsd     xmm0, qword ptr [rip + 0x130d0d6]
0037636a call      0x7ff7eade5c3c
0037636f movapd    xmm6, xmm0
00376373 mov       eax, dword ptr [rip + 0x1db786b]
00376379 test      al, 1
0037637b jne       0x7ff7ea2863b1
0037637d or        eax, 1
00376380 mov       dword ptr [rip + 0x1db785e], eax
00376386 mov       ecx, 0xa8
0037638b call      qword ptr [rip + 0x1ecfdef]
00376391 mov       qword ptr [rsp + 0x38], rax
00376396 test      rax, rax
00376399 je        0x7ff7ea2863a5
0037639b mov       rcx, rax
0037639e call      0x7ff7e9f369f9 ; ??0PortraitManager@@QEAA@XZ | ?_CONSTRUCTOR@PortraitManager@@QEAAPEAV1@XZ
003763a3 jmp       0x7ff7ea2863a8
003763a5 mov       rax, r12
003763a8 mov       qword ptr [rip + 0x1db7829], rax
003763af jmp       0x7ff7ea2863b8
003763b1 mov       rax, qword ptr [rip + 0x1db7820]
003763b8 mov       rcx, rax
003763bb call      0x7ff7e9f19020 ; ?saveTexture@PortraitManager@@QEAAXXZ
003763c0 mov       qword ptr [rsp + 0x98], 0xf
003763cc mov       qword ptr [rsp + 0x90], r12
003763d4 mov       byte ptr [rsp + 0x80], 0
003763dc mov       r8d, 0xa
003763e2 lea       rdx, [rip + 0x134cda7]
003763e9 lea       rcx, [rsp + 0x80]
003763f1 call      0x7ff7e9f5d7c5
003763f6 nop
003763f7 mov       eax, dword ptr [rip + 0x1db7817]
003763fd test      al, 1
003763ff jne       0x7ff7ea286434
00376401 or        eax, 1
00376404 mov       dword ptr [rip + 0x1db780a], eax
0037640a mov       ecx, 0x318
0037640f call      0x7ff7eade563a
00376414 mov       qword ptr [rsp + 0x38], rax
00376419 test      rax, rax
0037641c je        0x7ff7ea286428
0037641e mov       rcx, rax
00376421 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00376426 jmp       0x7ff7ea28642b
00376428 mov       rax, r12
0037642b mov       qword ptr [rip + 0x1db77d6], rax
00376432 jmp       0x7ff7ea28643b
00376434 mov       rax, qword ptr [rip + 0x1db77cd]
0037643b lea       r8, [rsp + 0x80]
00376443 lea       rdx, [rsp + 0x228]
0037644b mov       rcx, rax
0037644e call      0x7ff7e9f461ce ; ?writeFile@SaveFileSystem@@QEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
00376453 nop
00376454 cmp       qword ptr [rsp + 0x98], 0x10
0037645d jb        0x7ff7ea28646c
0037645f mov       rcx, qword ptr [rsp + 0x80]
00376467 call      0x7ff7eade5628
0037646c mov       qword ptr [rsp + 0x98], 0xf
00376478 mov       qword ptr [rsp + 0x90], r12
00376480 mov       byte ptr [rsp + 0x80], 0
00376488 lea       r8, [rip + 0x1dbcaa1]
0037648f lea       rdx, [rsp + 0x228]
00376497 lea       rcx, [rip + 0x1dbcf32]
0037649e call      0x7ff7e9f62685 ; ?save@GameDataContainer@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAVSerialisable@@@Z
003764a3 mov       r8, r15
003764a6 mov       rdx, qword ptr [rsp + 0x48]
003764ab lea       rcx, [rsp + 0xd0]
003764b3 call      0x7ff7e9f36945
003764b8 mov       rbx, rax
003764bb mov       eax, dword ptr [rip + 0x1db7753]
003764c1 test      al, 1
003764c3 jne       0x7ff7ea2864f8
003764c5 or        eax, 1
003764c8 mov       dword ptr [rip + 0x1db7746], eax
003764ce mov       ecx, 0x318
003764d3 call      0x7ff7eade563a
003764d8 mov       qword ptr [rsp + 0x38], rax
003764dd test      rax, rax
003764e0 je        0x7ff7ea2864ec
003764e2 mov       rcx, rax
003764e5 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
003764ea jmp       0x7ff7ea2864ef
003764ec mov       rax, r12
003764ef mov       qword ptr [rip + 0x1db7712], rax
003764f6 jmp       0x7ff7ea2864ff
003764f8 mov       rax, qword ptr [rip + 0x1db7709]
003764ff mov       rdx, rbx
00376502 mov       rcx, rax
00376505 call      0x7ff7e9f3fabd ; ?saveGame@SaveFileSystem@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
0037650a or        r14b, al
0037650d cmp       qword ptr [rsp + 0xe8], 0x10
00376516 jb        0x7ff7ea286525
00376518 mov       rcx, qword ptr [rsp + 0xd0]
00376520 call      0x7ff7eade5628
00376525 test      r14b, r14b
00376528 je        0x7ff7ea286808
0037652e xor       r9d, r9d
00376531 mov       esi, 6
00376536 mov       r8d, esi
00376539 mov       dword ptr [rsp + 0x28], r12d
0037653e mov       byte ptr [rsp + 0x20], 0x20
00376543 movapd    xmm1, xmm6
00376547 lea       rcx, [rsp + 0x120]
0037654f call      qword ptr [rip + 0x1ed1a23]
00376555 mov       rdi, rax
00376558 xor       r9d, r9d
0037655b mov       r8d, esi
0037655e mov       dword ptr [rsp + 0x28], r12d
00376563 mov       byte ptr [rsp + 0x20], 0x20
00376568 movapd    xmm1, xmm11
0037656d lea       rcx, [rsp + 0xd0]
00376575 call      qword ptr [rip + 0x1ed19fd]
0037657b mov       rbx, rax
0037657e lea       rcx, [rip + 0x1dbd3cb]
00376585 call      0x7ff7e9f4b8f9
0037658a mulsd     xmm0, xmm7
0037658e subsd     xmm0, xmm8
00376593 mulsd     xmm0, qword ptr [rip + 0x1341685]
0037659b addsd     xmm0, qword ptr [rip + 0x130ce9d]
003765a3 call      0x7ff7eade5c3c
003765a8 movapd    xmm1, xmm0
003765ac mov       dword ptr [rsp + 0x28], r12d
003765b1 mov       byte ptr [rsp + 0x20], 0x20
003765b6 movzx     r9d, r12w
003765ba movzx     r8d, si
003765be lea       rcx, [rsp + 0x160]
003765c6 call      qword ptr [rip + 0x1ed19ac]
003765cc nop
003765cd mov       r8, rax
003765d0 lea       rdx, [rip + 0x134d5c9]
003765d7 lea       rcx, [rsp + 0x200]
003765df call      0x7ff7e9f623dd
003765e4 nop
003765e5 lea       r8, [rip + 0x134d5ac]
003765ec mov       rdx, rax
003765ef lea       rcx, [rsp + 0x250]
003765f7 call      0x7ff7e9f42d6c
003765fc nop
003765fd mov       r8, rbx
00376600 mov       rdx, rax
00376603 lea       rcx, [rsp + 0xf8]
0037660b call      0x7ff7e9f5606a
00376610 nop
00376611 lea       r8, [rip + 0x134d578]
00376618 mov       rdx, rax
0037661b lea       rcx, [rsp + 0x188]
00376623 call      0x7ff7e9f42d6c
00376628 nop
00376629 mov       r8, rdi
0037662c mov       rdx, rax
0037662f lea       rcx, [rsp + 0x1b0]
00376637 call      0x7ff7e9f5606a
0037663c nop
0037663d lea       r8, [rip + 0x134d548]
00376644 mov       rdx, rax
00376647 lea       rcx, [rsp + 0xa8]
0037664f call      0x7ff7e9f42d6c
00376654 nop
00376655 mov       rdx, rax
00376658 mov       ecx, 0xffff80
0037665d call      0x7ff7e9f41a3e
00376662 nop
00376663 cmp       qword ptr [rsp + 0xc0], 0x10
0037666c jb        0x7ff7ea28667b
0037666e mov       rcx, qword ptr [rsp + 0xa8]
00376676 call      0x7ff7eade5628
0037667b mov       qword ptr [rsp + 0xc0], 0xf
00376687 mov       qword ptr [rsp + 0xb8], r12
0037668f mov       byte ptr [rsp + 0xa8], 0
00376697 cmp       qword ptr [rsp + 0x1c8], 0x10
003766a0 jb        0x7ff7ea2866af
003766a2 mov       rcx, qword ptr [rsp + 0x1b0]
003766aa call      0x7ff7eade5628
003766af mov       qword ptr [rsp + 0x1c8], 0xf
003766bb mov       qword ptr [rsp + 0x1c0], r12
003766c3 mov       byte ptr [rsp + 0x1b0], 0
003766cb cmp       qword ptr [rsp + 0x1a0], 0x10
003766d4 jb        0x7ff7ea2866e3
003766d6 mov       rcx, qword ptr [rsp + 0x188]
003766de call      0x7ff7eade5628
003766e3 mov       qword ptr [rsp + 0x1a0], 0xf
003766ef mov       qword ptr [rsp + 0x198], r12
003766f7 mov       byte ptr [rsp + 0x188], 0
003766ff cmp       qword ptr [rsp + 0x110], 0x10
00376708 jb        0x7ff7ea286717
0037670a mov       rcx, qword ptr [rsp + 0xf8]
00376712 call      0x7ff7eade5628
00376717 mov       qword ptr [rsp + 0x110], 0xf
00376723 mov       qword ptr [rsp + 0x108], r12
0037672b mov       byte ptr [rsp + 0xf8], 0
00376733 cmp       qword ptr [rsp + 0x268], 0x10
0037673c jb        0x7ff7ea28674b
0037673e mov       rcx, qword ptr [rsp + 0x250]
00376746 call      0x7ff7eade5628
0037674b mov       qword ptr [rsp + 0x268], 0xf
00376757 mov       qword ptr [rsp + 0x260], r12
0037675f mov       byte ptr [rsp + 0x250], 0
00376767 cmp       qword ptr [rsp + 0x218], 0x10
00376770 jb        0x7ff7ea28677f
00376772 mov       rcx, qword ptr [rsp + 0x200]
0037677a call      0x7ff7eade5628
0037677f mov       qword ptr [rsp + 0x218], 0xf
0037678b mov       qword ptr [rsp + 0x210], r12
00376793 mov       byte ptr [rsp + 0x200], 0
0037679b cmp       qword ptr [rsp + 0x178], 0x10
003767a4 jb        0x7ff7ea2867b3
003767a6 mov       rcx, qword ptr [rsp + 0x160]
003767ae call      0x7ff7eade5628
003767b3 mov       qword ptr [rsp + 0x178], 0xf
003767bf mov       qword ptr [rsp + 0x170], r12
003767c7 mov       byte ptr [rsp + 0x160], 0
003767cf cmp       qword ptr [rsp + 0xe8], 0x10
003767d8 jb        0x7ff7ea2867e7
003767da mov       rcx, qword ptr [rsp + 0xd0]
003767e2 call      0x7ff7eade5628
003767e7 mov       qword ptr [rsp + 0xe8], 0xf
003767f3 mov       qword ptr [rsp + 0xe0], r12
003767fb mov       byte ptr [rsp + 0xd0], 0
00376803 jmp       0x7ff7ea28698c
00376808 lea       rdx, [rip + 0x134d351]
0037680f lea       rcx, [rsp + 0x280]
00376817 call      0x7ff7e9f210b8
0037681c nop
0037681d lea       rdx, [rsp + 0xd0]
00376825 mov       rcx, rax
00376828 call      0x7ff7e9f18a3a
0037682d nop
0037682e xor       r8d, r8d
00376831 mov       rdx, rax
00376834 lea       rcx, [rip + 0x1dbc875]
0037683b call      0x7ff7e9f3b1ac ; ?showPlayerAMessage@GameWorld@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z
00376840 nop
00376841 cmp       qword ptr [rsp + 0xe8], 0x10
0037684a jb        0x7ff7ea286859
0037684c mov       rcx, qword ptr [rsp + 0xd0]
00376854 call      0x7ff7eade5628
00376859 mov       qword ptr [rsp + 0xe8], 0xf
00376865 mov       qword ptr [rsp + 0xe0], r12
0037686d mov       byte ptr [rsp + 0xd0], 0
00376875 lea       rcx, [rsp + 0x280]
0037687d call      0x7ff7e9f13c9c
00376882 mov       r8, r15
00376885 lea       rdx, [rip + 0x134d29c]
0037688c lea       rcx, [rsp + 0x120]
00376894 call      0x7ff7e9f3851f
00376899 nop
0037689a lea       r8, [rip + 0x134d277]
003768a1 mov       rdx, rax
003768a4 lea       rcx, [rsp + 0x200]
003768ac call      0x7ff7e9f42d6c
003768b1 nop
003768b2 lea       r8, [rip + 0x134d24f]
003768b9 mov       rdx, rax
003768bc lea       rcx, [rsp + 0x160]
003768c4 call      0x7ff7e9f42d6c
003768c9 nop
003768ca lea       r8, [rip + 0x130c2b7]
003768d1 mov       rdx, rax
003768d4 lea       rcx, [rsp + 0xd0]
003768dc call      0x7ff7e9f42d6c
003768e1 nop
003768e2 mov       edx, 4
003768e7 mov       rcx, rax
003768ea call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
003768ef nop
003768f0 cmp       qword ptr [rsp + 0xe8], 0x10
003768f9 jb        0x7ff7ea286908
003768fb mov       rcx, qword ptr [rsp + 0xd0]
00376903 call      0x7ff7eade5628
00376908 mov       qword ptr [rsp + 0xe8], 0xf
00376914 mov       qword ptr [rsp + 0xe0], r12
0037691c mov       byte ptr [rsp + 0xd0], 0
00376924 cmp       qword ptr [rsp + 0x178], 0x10
0037692d jb        0x7ff7ea28693c
0037692f mov       rcx, qword ptr [rsp + 0x160]
00376937 call      0x7ff7eade5628
0037693c mov       qword ptr [rsp + 0x178], 0xf
00376948 mov       qword ptr [rsp + 0x170], r12
00376950 mov       byte ptr [rsp + 0x160], 0
00376958 cmp       qword ptr [rsp + 0x218], 0x10
00376961 jb        0x7ff7ea286970
00376963 mov       rcx, qword ptr [rsp + 0x200]
0037696b call      0x7ff7eade5628
00376970 mov       qword ptr [rsp + 0x218], 0xf
0037697c mov       qword ptr [rsp + 0x210], r12
00376984 mov       byte ptr [rsp + 0x200], 0
0037698c cmp       qword ptr [rsp + 0x138], 0x10
00376995 jb        0x7ff7ea2869a5
00376997 mov       rcx, qword ptr [rsp + 0x120]
0037699f call      0x7ff7eade5628
003769a4 nop
003769a5 cmp       qword ptr [rsp + 0x240], 0x10
003769ae jb        0x7ff7ea2869bd
003769b0 mov       rcx, qword ptr [rsp + 0x228]
003769b8 call      0x7ff7eade5628
003769bd mov       qword ptr [rsp + 0x240], 0xf
003769c9 mov       qword ptr [rsp + 0x238], r12
003769d1 mov       byte ptr [rsp + 0x228], 0
003769d9 cmp       qword ptr [rsp + 0x1f0], 0x10
003769e2 jb        0x7ff7ea2869f1
003769e4 mov       rcx, qword ptr [rsp + 0x1d8]
003769ec call      0x7ff7eade5628
003769f1 xor       eax, eax
003769f3 jmp       0x7ff7ea2869fa
003769f5 mov       eax, 1
003769fa mov       rcx, qword ptr [rsp + 0x320]
00376a02 xor       rcx, rsp
00376a05 call      0x7ff7eade5db0
00376a0a lea       r11, [rsp + 0x370]
00376a12 movaps    xmm6, xmmword ptr [r11 - 0x10]
00376a17 movaps    xmm7, xmmword ptr [r11 - 0x20]
00376a1c movaps    xmm8, xmmword ptr [r11 - 0x30]
00376a21 movaps    xmm11, xmmword ptr [r11 - 0x40]
00376a26 mov       rsp, r11
00376a29 pop       r15
00376a2b pop       r14
00376a2d pop       r13
00376a2f pop       r12
00376a31 pop       rdi
00376a32 pop       rsi
00376a33 pop       rbx
00376a34 ret
00376a35 int3
