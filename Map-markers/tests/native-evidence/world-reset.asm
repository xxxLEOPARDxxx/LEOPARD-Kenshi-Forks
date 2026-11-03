?resetGame@GameWorld@@QEAAXXZ RVA=36ca40 slot=3588 PID=9388
0036ca40 mov       rax, rsp
0036ca43 push      rbp
0036ca44 lea       rbp, [rax - 0x68]
0036ca48 sub       rsp, 0x160
0036ca4f mov       qword ptr [rsp + 0x28], 0xfffffffffffffffe
0036ca58 mov       qword ptr [rax + 0x10], rbx
0036ca5c mov       qword ptr [rax + 0x18], rsi
0036ca60 mov       qword ptr [rax + 0x20], rdi
0036ca64 mov       rax, qword ptr [rip + 0x1da994d]
0036ca6b xor       rax, rsp
0036ca6e mov       qword ptr [rbp + 0x50], rax
0036ca72 mov       rsi, rcx
0036ca75 xor       ebx, ebx
0036ca77 mov       dword ptr [rsp + 0x20], ebx
0036ca7b mov       qword ptr [rsp + 0x48], 0xf
0036ca84 mov       qword ptr [rsp + 0x40], rbx
0036ca89 mov       byte ptr [rsp + 0x30], bl
0036ca8d lea       r8d, [rbx + 0xa]
0036ca91 lea       rdx, [rip + 0x13567a8]
0036ca98 lea       rcx, [rsp + 0x30]
0036ca9d call      0x7ff7e9f5d7c5
0036caa2 nop
0036caa3 lea       edx, [rbx + 2]
0036caa6 lea       rcx, [rsp + 0x30]
0036caab call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0036cab0 nop
0036cab1 cmp       qword ptr [rsp + 0x48], 0x10
0036cab7 jb        0x7ff7ea27cac3
0036cab9 mov       rcx, qword ptr [rsp + 0x30]
0036cabe call      0x7ff7eade5628
0036cac3 mov       byte ptr [rsi + 0x8ba], 1
0036caca mov       eax, dword ptr [rip + 0x1dc1144]
0036cad0 test      al, 1
0036cad2 jne       0x7ff7ea27cb07
0036cad4 or        eax, 1
0036cad7 mov       dword ptr [rip + 0x1dc1137], eax
0036cadd mov       ecx, 0x318
0036cae2 call      0x7ff7eade563a
0036cae7 mov       qword ptr [rsp + 0x20], rax
0036caec test      rax, rax
0036caef je        0x7ff7ea27cafb
0036caf1 mov       rcx, rax
0036caf4 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
0036caf9 jmp       0x7ff7ea27cafe
0036cafb mov       rax, rbx
0036cafe mov       qword ptr [rip + 0x1dc1103], rax
0036cb05 jmp       0x7ff7ea27cb0e
0036cb07 mov       rax, qword ptr [rip + 0x1dc10fa]
0036cb0e mov       rcx, rax
0036cb11 call      0x7ff7e9f29835 ; ?newGame@SaveFileSystem@@QEAAXXZ
0036cb16 mov       r11d, dword ptr [rip + 0x1dc10c7]
0036cb1d test      r11b, 1
0036cb21 jne       0x7ff7ea27cb59
0036cb23 or        r11d, 1
0036cb27 mov       dword ptr [rip + 0x1dc10b6], r11d
0036cb2e mov       ecx, 0xa8
0036cb33 call      qword ptr [rip + 0x1ed9647]
0036cb39 mov       qword ptr [rsp + 0x20], rax
0036cb3e test      rax, rax
0036cb41 je        0x7ff7ea27cb4d
0036cb43 mov       rcx, rax
0036cb46 call      0x7ff7e9f369f9 ; ??0PortraitManager@@QEAA@XZ | ?_CONSTRUCTOR@PortraitManager@@QEAAPEAV1@XZ
0036cb4b jmp       0x7ff7ea27cb50
0036cb4d mov       rax, rbx
0036cb50 mov       qword ptr [rip + 0x1dc1081], rax
0036cb57 jmp       0x7ff7ea27cb60
0036cb59 mov       rax, qword ptr [rip + 0x1dc1078]
0036cb60 xor       edx, edx
0036cb62 mov       rcx, rax
0036cb65 call      0x7ff7e9f1e237 ; ?clearPortraits@PortraitManager@@QEAAX_N@Z
0036cb6a xor       r9d, r9d
0036cb6d xor       r8d, r8d
0036cb70 xor       edx, edx
0036cb72 lea       rcx, [rip + 0x1dc6537]
0036cb79 call      0x7ff7e9f2c341 ; ?populateMapArea_nonPermanent@GameWorld@@QEAAXPEAVZoneMap@@H_N@Z
0036cb7e lea       rcx, [rip + 0x1dc5bcb]
0036cb85 call      0x7ff7e9f39c08 ; ?init@ForgottenGUI@@QEAAXXZ
0036cb8a lea       rcx, [rip + 0x1dc5bbf]
0036cb91 call      0x7ff7e9f2384f ; ?clearGUI@ForgottenGUI@@QEAAXXZ
0036cb96 lea       rcx, [rip + 0x1dc6513]
0036cb9d call      0x7ff7e9f4f8ff ; ?destroyDeathParade@GameWorld@@QEAAXXZ
0036cba2 mov       rcx, qword ptr [rip + 0x1dc69bf]
0036cba9 call      0x7ff7e9f23340
0036cbae mov       rcx, qword ptr [rip + 0x1dc6a7b]
0036cbb5 call      0x7ff7e9f1cdbf ; ?clearAndReset@PlayerInterface@@QEAAXXZ
0036cbba mov       rcx, qword ptr [rip + 0x1dc6997]
0036cbc1 test      rcx, rcx
0036cbc4 je        0x7ff7ea27cbcb
0036cbc6 call      0x7ff7e9f28db3 ; ?clearAndDestroy@FactionManager@@QEAAXXZ
0036cbcb lea       rcx, [rip + 0x1dc6c2e]
0036cbd2 call      0x7ff7e9f33de4
0036cbd7 mov       rcx, qword ptr [rip + 0x1dc64ba]
0036cbde call      0x7ff7e9f1a07e
0036cbe3 mov       rcx, qword ptr [rip + 0x1dc6a46]
0036cbea mov       rcx, qword ptr [rcx + 0x30]
0036cbee call      0x7ff7e9f605dd ; ?reset@CameraClass@@QEAAXXZ
0036cbf3 mov       r11d, dword ptr [rip + 0x1dc0ffa]
0036cbfa test      r11b, 1
0036cbfe jne       0x7ff7ea27cc35
0036cc00 or        r11d, 1
0036cc04 mov       dword ptr [rip + 0x1dc0fe9], r11d
0036cc0b mov       ecx, 0x40
0036cc10 call      0x7ff7eade563a
0036cc15 mov       qword ptr [rsp + 0x20], rax
0036cc1a test      rax, rax
0036cc1d je        0x7ff7ea27cc29
0036cc1f mov       rcx, rax
0036cc22 call      0x7ff7e9f3c598
0036cc27 jmp       0x7ff7ea27cc2c
0036cc29 mov       rax, rbx
0036cc2c mov       qword ptr [rip + 0x1dc0fb5], rax
0036cc33 jmp       0x7ff7ea27cc3c
0036cc35 mov       rax, qword ptr [rip + 0x1dc0fac]
0036cc3c mov       rcx, rax
0036cc3f call      0x7ff7e9f1e3b3
0036cc44 lea       rcx, [rip + 0x1dc6785]
0036cc4b call      0x7ff7e9f29704 ; ?clearInstances@GameDataContainer@@QEAAXXZ
0036cc50 lea       rcx, [rip + 0x1dc6779]
0036cc57 call      0x7ff7e9f24556 ; ?clearAndDestroy@GameDataContainer@@QEAAXXZ
0036cc5c mov       r11, qword ptr [rip + 0x1dc69cd]
0036cc63 mov       rcx, qword ptr [r11 + 0x2a0]
0036cc6a call      0x7ff7e9f1418d ; ?clearAndDestroy@Faction@@QEAAXXZ
0036cc6f mov       qword ptr [rsp + 0x48], 0xf
0036cc78 mov       qword ptr [rsp + 0x40], rbx
0036cc7d mov       byte ptr [rsp + 0x30], 0
0036cc82 mov       r8d, 0x11
0036cc88 lea       rdx, [rip + 0x1356599]
0036cc8f lea       rcx, [rsp + 0x30]
0036cc94 call      0x7ff7e9f5d7c5
0036cc99 nop
0036cc9a lea       rdx, [rsp + 0x30]
0036cc9f lea       rcx, [rip + 0x1dc642a]
0036cca6 call      0x7ff7e9f24790 ; ?getData@GameDataContainer@@QEAAPEAVGameData@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
0036ccab mov       rdi, rax
0036ccae cmp       qword ptr [rsp + 0x48], 0x10
0036ccb4 jb        0x7ff7ea27ccc0
0036ccb6 mov       rcx, qword ptr [rsp + 0x30]
0036ccbb call      0x7ff7eade5628
0036ccc0 mov       qword ptr [rsp + 0x48], 0xf
0036ccc9 mov       qword ptr [rsp + 0x40], rbx
0036ccce mov       byte ptr [rsp + 0x30], 0
0036ccd3 test      rdi, rdi
0036ccd6 je        0x7ff7ea27cd0c
0036ccd8 mov       qword ptr [rsp + 0x70], 0xf
0036cce1 mov       qword ptr [rsp + 0x68], rbx
0036cce6 mov       byte ptr [rsp + 0x58], 0
0036cceb lea       rdx, [rdi + 0x28]
0036ccef or        r9, 0xffffffffffffffff
0036ccf3 xor       r8d, r8d
0036ccf6 lea       rcx, [rsp + 0x58]
0036ccfb call      0x7ff7e9f3808d
0036cd00 lea       rax, [rsp + 0x58]
0036cd05 mov       edi, 1
0036cd0a jmp       0x7ff7ea27cd37
0036cd0c lea       rdx, [rip + 0x1356505]
0036cd13 lea       rcx, [rbp - 0x50]
0036cd17 call      0x7ff7e9f210b8
0036cd1c nop
0036cd1d mov       dword ptr [rsp + 0x20], 2
0036cd25 lea       rdx, [rbp - 0x80]
0036cd29 mov       rcx, rax
0036cd2c call      0x7ff7e9f18a3a
0036cd31 nop
0036cd32 mov       edi, 6
0036cd37 mov       dword ptr [rsp + 0x20], edi
0036cd3b mov       rcx, qword ptr [rip + 0x1dc68ee]
0036cd42 add       rcx, 8
0036cd46 mov       rdx, rax
0036cd49 call      0x7ff7e9f121a3
0036cd4e nop
0036cd4f test      dil, 4
0036cd53 je        0x7ff7ea27cd78
0036cd55 and       edi, 0xfffffffb
0036cd58 cmp       qword ptr [rbp - 0x68], 0x10
0036cd5d jb        0x7ff7ea27cd68
0036cd5f mov       rcx, qword ptr [rbp - 0x80]
0036cd63 call      0x7ff7eade5628
0036cd68 mov       qword ptr [rbp - 0x68], 0xf
0036cd70 mov       qword ptr [rbp - 0x70], rbx
0036cd74 mov       byte ptr [rbp - 0x80], 0
0036cd78 test      dil, 2
0036cd7c je        0x7ff7ea27cde1
0036cd7e and       edi, 0xfffffffd
0036cd81 cmp       qword ptr [rbp + 0x38], 0x10
0036cd86 jb        0x7ff7ea27cd91
0036cd88 mov       rcx, qword ptr [rbp + 0x20]
0036cd8c call      0x7ff7eade5628
0036cd91 mov       qword ptr [rbp + 0x38], 0xf
0036cd99 mov       qword ptr [rbp + 0x30], rbx
0036cd9d mov       byte ptr [rbp + 0x20], 0
0036cda1 cmp       qword ptr [rbp + 0x10], 0x10
0036cda6 jb        0x7ff7ea27cdb1
0036cda8 mov       rcx, qword ptr [rbp - 8]
0036cdac call      0x7ff7eade5628
0036cdb1 mov       qword ptr [rbp + 0x10], 0xf
0036cdb9 mov       qword ptr [rbp + 8], rbx
0036cdbd mov       byte ptr [rbp - 8], 0
0036cdc1 cmp       qword ptr [rbp - 0x18], 0x10
0036cdc6 jb        0x7ff7ea27cdd1
0036cdc8 mov       rcx, qword ptr [rbp - 0x30]
0036cdcc call      0x7ff7eade5628
0036cdd1 mov       qword ptr [rbp - 0x18], 0xf
0036cdd9 mov       qword ptr [rbp - 0x20], rbx
0036cddd mov       byte ptr [rbp - 0x30], 0
0036cde1 test      dil, 1
0036cde5 je        0x7ff7ea27cdf9
0036cde7 cmp       qword ptr [rsp + 0x70], 0x10
0036cded jb        0x7ff7ea27cdf9
0036cdef mov       rcx, qword ptr [rsp + 0x58]
0036cdf4 call      0x7ff7eade5628
0036cdf9 mov       rax, qword ptr [rip + 0x1dc6830]
0036ce00 mov       rcx, qword ptr [rax + 0x2a0]
0036ce07 lea       rdx, [rax + 8]
0036ce0b add       rcx, 0x1a8
0036ce12 or        r9, 0xffffffffffffffff
0036ce16 xor       r8d, r8d
0036ce19 call      0x7ff7e9f3808d
0036ce1e mov       r11, qword ptr [rip + 0x1dc680b]
0036ce25 mov       rcx, qword ptr [r11 + 0x2a0]
0036ce2c call      0x7ff7e9f153f8 ; ?resetPlatoonID@Faction@@QEAAXXZ
0036ce31 mov       rcx, qword ptr [rip + 0x1dc6b28]
0036ce38 call      0x7ff7e9f613fc ; ?deactivateAllActiveZones@ZoneManager@@QEAAXXZ
0036ce3d mov       rcx, qword ptr [rip + 0x1dc6b1c]
0036ce44 call      0x7ff7e9f38295 ; ?resetStates@ZoneManager@@QEAAXXZ
0036ce49 mov       rcx, qword ptr [rsi + 0x4e0]
0036ce50 call      0x7ff7e9f5eada
0036ce55 mov       rcx, qword ptr [rip + 0x1dc67d4]
0036ce5c mov       rcx, qword ptr [rcx + 0x38]
0036ce60 call      0x7ff7e9f41c5a ; ?_reset@Research@@QEAAXXZ
0036ce65 call      0x7ff7e9f27cba ; ?newGameReset@DialogDataManager@@SAXXZ
0036ce6a xorps     xmm1, xmm1
0036ce6d mov       rcx, qword ptr [rip + 0x1dc254c]
0036ce74 call      0x7ff7e9f30acc
0036ce79 mov       r11, qword ptr [rip + 0x1dc2540]
0036ce80 mov       dword ptr [r11 + 8], 1
0036ce88 mov       rax, qword ptr [rip + 0x1dc1659]
0036ce8f test      rax, rax
0036ce92 jne       0x7ff7ea27cebc
0036ce94 mov       ecx, 0x2d8
0036ce99 call      0x7ff7eade563a
0036ce9e mov       qword ptr [rsp + 0x20], rax
0036cea3 test      rax, rax
0036cea6 je        0x7ff7ea27ceb2
0036cea8 mov       rcx, rax
0036ceab call      0x7ff7e9f5bf83 ; ??0ManagementScreen@@QEAA@XZ | ?_CONSTRUCTOR@ManagementScreen@@QEAAPEAV1@XZ
0036ceb0 jmp       0x7ff7ea27ceb5
0036ceb2 mov       rax, rbx
0036ceb5 mov       qword ptr [rip + 0x1dc162c], rax
0036cebc mov       rcx, rax
0036cebf call      0x7ff7e9f1376a ; ?clear@ManagementScreen@@QEAAXXZ
0036cec4 call      0x7ff7e9f115fa ; ?clearDelayedItemLoadingMessages@TownBase@@SAXXZ
0036cec9 call      0x7ff7e9f2c841
0036cece mov       rcx, rax
0036ced1 call      0x7ff7e9f36a0d
0036ced6 mov       r11d, dword ptr [rip + 0x1dc0d27]
0036cedd test      r11b, 1
0036cee1 jne       0x7ff7ea27cf17
0036cee3 or        r11d, 1
0036cee7 mov       dword ptr [rip + 0x1dc0d16], r11d
0036ceee mov       ecx, 0x88
0036cef3 call      qword ptr [rip + 0x1ed9287]
0036cef9 mov       qword ptr [rsp + 0x20], rax
0036cefe test      rax, rax
0036cf01 je        0x7ff7ea27cf0e
0036cf03 mov       rcx, rax
0036cf06 call      0x7ff7e9f64232
0036cf0b mov       rbx, rax
0036cf0e mov       qword ptr [rip + 0x1dc0ce3], rbx
0036cf15 jmp       0x7ff7ea27cf1e
0036cf17 mov       rbx, qword ptr [rip + 0x1dc0cda]
0036cf1e mov       rcx, rbx
0036cf21 call      0x7ff7e9f130b2
0036cf26 call      0x7ff7e9f5c65e ; ?resetAllTheStaticData@Blackboard@@SAXXZ
0036cf2b lea       rcx, [rip + 0x1dc617e]
0036cf32 call      0x7ff7e9f52a05 ; ?flushKillList@GameWorld@@QEAAXXZ
0036cf37 lea       rcx, [rip + 0x1dc1292]
0036cf3e call      0x7ff7e9f1cd38
0036cf43 mov       rcx, qword ptr [rip + 0x1dc6156]
0036cf4a call      0x7ff7e9f408af ; ?_resetGame@TownList@@QEAAXXZ
0036cf4f lea       rcx, [rip + 0x1dc5fe2]
0036cf56 mov       rax, qword ptr [rip + 0x1dc5fdb]
0036cf5d call      qword ptr [rax + 0x18]
0036cf60 lea       rcx, [rip + 0x1dc5fe9]
0036cf67 mov       rax, qword ptr [rip + 0x1dc5fe2]
0036cf6e call      qword ptr [rax + 0x18]
0036cf71 lea       rcx, [rip + 0x1dc5ff0]
0036cf78 mov       rax, qword ptr [rip + 0x1dc5fe9]
0036cf7f call      qword ptr [rax + 0x18]
0036cf82 lea       rcx, [rip + 0x1dc5ff7]
0036cf89 mov       rax, qword ptr [rip + 0x1dc5ff0]
0036cf90 call      qword ptr [rax + 0x18]
0036cf93 lea       rcx, [rip + 0x1dc6046]
0036cf9a mov       rax, qword ptr [rip + 0x1dc603f]
0036cfa1 call      qword ptr [rax + 0x18]
0036cfa4 lea       rcx, [rip + 0x1dc6095]
0036cfab mov       rax, qword ptr [rip + 0x1dc608e]
0036cfb2 call      qword ptr [rax + 0x18]
0036cfb5 lea       rcx, [rip + 0x1dc609c]
0036cfbc call      0x7ff7e9f3a62b
0036cfc1 lea       rcx, [rip + 0x1dc5788]
0036cfc8 call      0x7ff7e9f3670b ; ?update@ForgottenGUI@@QEAAXXZ
0036cfcd xor       edx, edx
0036cfcf lea       rcx, [rip + 0x1dc60da]
0036cfd6 call      0x7ff7e9f2ee43 ; ?userPause@GameWorld@@QEAAX_N@Z
0036cfdb mov       byte ptr [rsi + 0x8ba], 0
0036cfe2 mov       rcx, qword ptr [rbp + 0x50]
0036cfe6 xor       rcx, rsp
0036cfe9 call      0x7ff7eade5db0
0036cfee lea       r11, [rsp + 0x160]
0036cff6 mov       rbx, qword ptr [r11 + 0x18]
0036cffa mov       rsi, qword ptr [r11 + 0x20]
0036cffe mov       rdi, qword ptr [r11 + 0x28]
0036d002 mov       rsp, r11
0036d005 pop       rbp
0036d006 ret
0036d007 int3
