?initialisation@GameWorld@@QEAA_NXZ RVA=86ccb0 slot=3590 PID=9388
0086ccb0 mov       rax, rsp
0086ccb3 push      rbp
0086ccb4 push      rdi
0086ccb5 push      r12
0086ccb7 push      r13
0086ccb9 push      r14
0086ccbb lea       rbp, [rax - 0x58]
0086ccbf sub       rsp, 0x130
0086ccc6 mov       qword ptr [rsp + 0x28], 0xfffffffffffffffe
0086cccf mov       qword ptr [rax + 0x10], rbx
0086ccd3 mov       qword ptr [rax + 0x18], rsi
0086ccd7 mov       rax, qword ptr [rip + 0x18a96da]
0086ccde xor       rax, rsp
0086cce1 mov       qword ptr [rbp + 0x20], rax
0086cce5 mov       rdi, rcx
0086cce8 cmp       byte ptr [rcx + 0xc], 0
0086ccec jne       0x7ff7ea77d0d3
0086ccf2 mov       byte ptr [rcx + 0xc], 1
0086ccf6 mov       rcx, qword ptr [rcx + 0x18]
0086ccfa call      0x7ff7e9f2f208 ; ?getBackthread@PhysicsInterface@@QEAAPEAVPhysicsActual@@XZ
0086ccff mov       rcx, rax
0086cd02 call      0x7ff7e9f25bf4 ; ?setup@PhysicsActual@@QEAA_NXZ
0086cd07 test      al, al
0086cd09 jne       0x7ff7ea77cd60
0086cd0b mov       qword ptr [rsp + 0x48], 0xf
0086cd14 xor       ebx, ebx
0086cd16 mov       qword ptr [rsp + 0x40], rbx
0086cd1b mov       byte ptr [rsp + 0x30], bl
0086cd1f lea       r8d, [rbx + 0x2c]
0086cd23 lea       rdx, [rip + 0xeb494e]
0086cd2a lea       rcx, [rsp + 0x30]
0086cd2f call      0x7ff7e9f5d7c5
0086cd34 nop
0086cd35 lea       edx, [rbx + 5]
0086cd38 lea       rcx, [rsp + 0x30]
0086cd3d call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
0086cd42 nop
0086cd43 cmp       qword ptr [rsp + 0x48], 0x10
0086cd49 jb        0x7ff7ea77cd55
0086cd4b mov       rcx, qword ptr [rsp + 0x30]
0086cd50 call      0x7ff7eade5628
0086cd55 mov       byte ptr [rdi + 0xc], 0
0086cd59 xor       al, al
0086cd5b jmp       0x7ff7ea77d0d5
0086cd60 mov       ecx, 0x180
0086cd65 call      0x7ff7eade563a
0086cd6a mov       qword ptr [rsp + 0x20], rax
0086cd6f xor       ebx, ebx
0086cd71 test      rax, rax
0086cd74 je        0x7ff7ea77cd83
0086cd76 mov       rcx, rax
0086cd79 call      0x7ff7e9f50b6a ; ??0Research@@QEAA@XZ | ?_CONSTRUCTOR@Research@@QEAAPEAV1@XZ
0086cd7e mov       rcx, rax
0086cd81 jmp       0x7ff7ea77cd86
0086cd83 mov       rcx, rbx
0086cd86 mov       rax, qword ptr [rip + 0x18c68a3]
0086cd8d mov       qword ptr [rax + 0x38], rcx
0086cd91 call      0x7ff7e9f42d12 ; ?_setup@Research@@QEAAXXZ
0086cd96 call      0x7ff7e9f3db41
0086cd9b call      0x7ff7e9f5a36d
0086cda0 mov       rcx, rax
0086cda3 call      0x7ff7e9f374d5
0086cda8 mov       ecx, 0x50
0086cdad call      qword ptr [rip + 0x19d93cd]
0086cdb3 mov       qword ptr [rsp + 0x20], rax
0086cdb8 test      rax, rax
0086cdbb je        0x7ff7ea77cdc7
0086cdbd mov       rcx, rax
0086cdc0 call      0x7ff7e9f1d5f8 ; ??0FactionManager@@QEAA@XZ | ?_CONSTRUCTOR@FactionManager@@QEAAPEAV1@XZ
0086cdc5 jmp       0x7ff7ea77cdca
0086cdc7 mov       rax, rbx
0086cdca mov       qword ptr [rdi + 0x4a8], rax
0086cdd1 mov       ecx, 0x1682c0
0086cdd6 call      qword ptr [rip + 0x19d93a4]
0086cddc mov       qword ptr [rsp + 0x20], rax
0086cde1 test      rax, rax
0086cde4 je        0x7ff7ea77cdf0
0086cde6 mov       rcx, rax
0086cde9 call      0x7ff7e9f3653f ; ??0ZoneManager@@QEAA@XZ | ?_CONSTRUCTOR@ZoneManager@@QEAAPEAV1@XZ
0086cdee jmp       0x7ff7ea77cdf3
0086cdf0 mov       rax, rbx
0086cdf3 mov       qword ptr [rdi + 0x8b0], rax
0086cdfa mov       rcx, rax
0086cdfd call      0x7ff7e9f15ab5 ; ?setup@ZoneManager@@QEAAXXZ
0086ce02 call      0x7ff7e9f432b7 ; ?setup@CombatClass@@SAXXZ
0086ce07 mov       r11, qword ptr [rdi + 0x8b0]
0086ce0e mov       qword ptr [rip + 0x18c6293], r11
0086ce15 mov       ecx, 0x2c0
0086ce1a call      0x7ff7eade563a
0086ce1f mov       qword ptr [rsp + 0x20], rax
0086ce24 test      rax, rax
0086ce27 je        0x7ff7ea77ce33
0086ce29 mov       rcx, rax
0086ce2c call      0x7ff7e9f42aa1 ; ??0NavMesh@@QEAA@XZ | ?_CONSTRUCTOR@NavMesh@@QEAAPEAV1@XZ
0086ce31 jmp       0x7ff7ea77ce36
0086ce33 mov       rax, rbx
0086ce36 mov       qword ptr [rdi + 0x4b0], rax
0086ce3d mov       rcx, rax
0086ce40 call      0x7ff7e9f218e2 ; ?initialise@NavMesh@@QEAAXXZ
0086ce45 mov       ecx, 0xc0
0086ce4a call      0x7ff7eade563a
0086ce4f mov       qword ptr [rsp + 0x20], rax
0086ce54 test      rax, rax
0086ce57 je        0x7ff7ea77ce62
0086ce59 mov       rcx, rax
0086ce5c call      0x7ff7e9f5c10e
0086ce61 nop
0086ce62 call      0x7ff7e9f47952
0086ce67 mov       rsi, rax
0086ce6a mov       ecx, 0x200
0086ce6f call      0x7ff7eade563a
0086ce74 mov       qword ptr [rsp + 0x20], rax
0086ce79 test      rax, rax
0086ce7c je        0x7ff7ea77ce88
0086ce7e mov       rcx, rax
0086ce81 call      0x7ff7e9f24998
0086ce86 jmp       0x7ff7ea77ce8b
0086ce88 mov       rax, rbx
0086ce8b mov       qword ptr [rdi + 0x790], rax
0086ce92 mov       rcx, qword ptr [rip + 0x18c61ff]
0086ce99 call      0x7ff7e9f1e8b3
0086ce9e mov       qword ptr [rsp + 0x48], 0xf
0086cea7 mov       qword ptr [rsp + 0x40], rbx
0086ceac mov       byte ptr [rsp + 0x30], 0
0086ceb1 mov       r8d, 0x11
0086ceb7 lea       rdx, [rip + 0xe5636a]
0086cebe lea       rcx, [rsp + 0x30]
0086cec3 call      0x7ff7e9f5d7c5
0086cec8 nop
0086cec9 lea       rdx, [rip + 0xe56348]
0086ced0 lea       rcx, [rbp - 0x80]
0086ced4 call      0x7ff7e9f210b8
0086ced9 nop
0086ceda lea       rdx, [rsp + 0x58]
0086cedf mov       rcx, rax
0086cee2 call      0x7ff7e9f18a3a
0086cee7 nop
0086cee8 mov       r8, rax
0086ceeb lea       rdx, [rsp + 0x30]
0086cef0 mov       rcx, qword ptr [rdi + 0x4a8]
0086cef7 call      0x7ff7e9f546bb ; ?getOrCreateFaction@FactionManager@@QEAAPEAVFaction@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z
0086cefc mov       rdx, rax
0086ceff mov       rcx, qword ptr [rdi + 0x580]
0086cf06 call      0x7ff7e9f2e0dd ; ?setFaction@PlayerInterface@@QEAAXPEAVFaction@@@Z
0086cf0b nop
0086cf0c cmp       qword ptr [rsp + 0x70], 0x10
0086cf12 jb        0x7ff7ea77cf1e
0086cf14 mov       rcx, qword ptr [rsp + 0x58]
0086cf19 call      0x7ff7eade5628
0086cf1e mov       qword ptr [rsp + 0x70], 0xf
0086cf27 mov       qword ptr [rsp + 0x68], rbx
0086cf2c mov       byte ptr [rsp + 0x58], 0
0086cf31 cmp       qword ptr [rbp + 8], 0x10
0086cf36 jb        0x7ff7ea77cf41
0086cf38 mov       rcx, qword ptr [rbp - 0x10]
0086cf3c call      0x7ff7eade5628
0086cf41 mov       qword ptr [rbp + 8], 0xf
0086cf49 mov       qword ptr [rbp], rbx
0086cf4d mov       byte ptr [rbp - 0x10], 0
0086cf51 cmp       qword ptr [rbp - 0x20], 0x10
0086cf56 jb        0x7ff7ea77cf61
0086cf58 mov       rcx, qword ptr [rbp - 0x38]
0086cf5c call      0x7ff7eade5628
0086cf61 mov       qword ptr [rbp - 0x20], 0xf
0086cf69 mov       qword ptr [rbp - 0x28], rbx
0086cf6d mov       byte ptr [rbp - 0x38], 0
0086cf71 cmp       qword ptr [rbp - 0x48], 0x10
0086cf76 jb        0x7ff7ea77cf81
0086cf78 mov       rcx, qword ptr [rbp - 0x60]
0086cf7c call      0x7ff7eade5628
0086cf81 mov       qword ptr [rbp - 0x48], 0xf
0086cf89 mov       qword ptr [rbp - 0x50], rbx
0086cf8d mov       byte ptr [rbp - 0x60], 0
0086cf91 cmp       qword ptr [rsp + 0x48], 0x10
0086cf97 jb        0x7ff7ea77cfa3
0086cf99 mov       rcx, qword ptr [rsp + 0x30]
0086cf9e call      0x7ff7eade5628
0086cfa3 lea       rdx, [rdi + 0x1a0]
0086cfaa mov       rcx, rdi
0086cfad call      0x7ff7e9f38c68 ; ?initialiseNewGameWorld@GameWorld@@QEAAXPEAVGameDataManager@@@Z
0086cfb2 test      rsi, rsi
0086cfb5 je        0x7ff7ea77cfbf
0086cfb7 mov       rcx, rsi
0086cfba call      0x7ff7e9f36ecc
0086cfbf call      0x7ff7e9f23f84
0086cfc4 call      0x7ff7e9f17676
0086cfc9 mov       rcx, qword ptr [rip + 0x18c60c8]
0086cfd0 call      0x7ff7e9f25a0f
0086cfd5 mov       r13, qword ptr [rdi + 0x4a8]
0086cfdc mov       r12d, ebx
0086cfdf cmp       dword ptr [r13 + 8], ebx
0086cfe3 jbe       0x7ff7ea77d011
0086cfe5 mov       rsi, rbx
0086cfe8 nop       dword ptr [rax + rax]
0086cff0 mov       rax, qword ptr [r13 + 0x10]
0086cff4 mov       rcx, qword ptr [rax + rsi]
0086cff8 mov       rcx, qword ptr [rcx + 0x88]
0086cfff call      0x7ff7e9f3eb4f ; ?setup@FactionWarMgr@@QEAAXXZ
0086d004 inc       r12d
0086d007 add       rsi, 8
0086d00b cmp       r12d, dword ptr [r13 + 8]
0086d00f jb        0x7ff7ea77cff0
0086d011 lea       rdx, [rdi + 0x1a0]
0086d018 mov       rcx, qword ptr [rip + 0x18c1441]
0086d01f call      0x7ff7e9f2eb9b
0086d024 mov       qword ptr [rsp + 0x48], 0xf
0086d02d mov       qword ptr [rsp + 0x40], rbx
0086d032 mov       byte ptr [rsp + 0x30], 0
0086d037 mov       r8d, 0xf
0086d03d lea       rdx, [rip + 0xea3844]
0086d044 lea       rcx, [rsp + 0x30]
0086d049 call      0x7ff7e9f5d7c5
0086d04e nop
0086d04f lea       rdx, [rsp + 0x30]
0086d054 mov       rcx, qword ptr [rip + 0x18ba2ed]
0086d05b call      0x7ff7e9f24ee3
0086d060 nop
0086d061 cmp       qword ptr [rsp + 0x48], 0x10
0086d067 jb        0x7ff7ea77d073
0086d069 mov       rcx, qword ptr [rsp + 0x30]
0086d06e call      0x7ff7eade5628
0086d073 mov       eax, dword ptr [rip + 0x18bfddb]
0086d079 test      al, 1
0086d07b jne       0x7ff7ea77d0b1
0086d07d or        eax, 1
0086d080 mov       dword ptr [rip + 0x18bfdce], eax
0086d086 mov       ecx, 0xf8
0086d08b call      qword ptr [rip + 0x19d90ef]
0086d091 mov       qword ptr [rsp + 0x20], rax
0086d096 test      rax, rax
0086d099 je        0x7ff7ea77d0a5
0086d09b mov       rcx, rax
0086d09e call      0x7ff7e9f56074
0086d0a3 jmp       0x7ff7ea77d0a8
0086d0a5 mov       rax, rbx
0086d0a8 mov       qword ptr [rip + 0x18bfd99], rax
0086d0af jmp       0x7ff7ea77d0b8
0086d0b1 mov       rax, qword ptr [rip + 0x18bfd90]
0086d0b8 mov       rcx, rax
0086d0bb call      0x7ff7e9f56d62
0086d0c0 mov       rcx, rdi
0086d0c3 call      0x7ff7e9f5c7c1 ; ?startUpThreads@GameWorld@@QEAA_NXZ
0086d0c8 test      al, al
0086d0ca jne       0x7ff7ea77d0d3
0086d0cc inc       ebx
0086d0ce cmp       ebx, 6
0086d0d1 jl        0x7ff7ea77d0c0
0086d0d3 mov       al, 1
0086d0d5 mov       rcx, qword ptr [rbp + 0x20]
0086d0d9 xor       rcx, rsp
0086d0dc call      0x7ff7eade5db0
0086d0e1 lea       r11, [rsp + 0x130]
0086d0e9 mov       rbx, qword ptr [r11 + 0x38]
0086d0ed mov       rsi, qword ptr [r11 + 0x40]
0086d0f1 mov       rsp, r11
0086d0f4 pop       r14
0086d0f6 pop       r13
0086d0f8 pop       r12
0086d0fa pop       rdi
0086d0fb pop       rbp
0086d0fc ret
0086d0fd int3
