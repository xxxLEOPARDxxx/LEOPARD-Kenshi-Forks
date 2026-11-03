?_NV_threadProc@SaveFileSystem@@QEAAKXZ RVA=472720 slot=6178 PID=9388
00472720 mov       rax, rsp
00472723 push      rdi
00472724 push      r12
00472726 push      r13
00472728 push      r14
0047272a push      r15
0047272c sub       rsp, 0x470
00472733 mov       qword ptr [rsp + 0x60], 0xfffffffffffffffe
0047273c mov       qword ptr [rax + 0x10], rbx
00472740 mov       qword ptr [rax + 0x18], rsi
00472744 mov       rax, qword ptr [rip + 0x1ca3c6d]
0047274b xor       rax, rsp
0047274e mov       qword ptr [rsp + 0x460], rax
00472756 mov       r14, rcx
00472759 mov       qword ptr [rsp + 0x38], rcx
0047275e lea       rcx, [rip + 0x1cc11eb]
00472765 call      0x7ff7e9f4b8f9
0047276a mulsd     xmm0, qword ptr [rip + 0x12375be]
00472772 movsd     qword ptr [rsp + 0x50], xmm0
00472778 xor       esi, esi
0047277a mov       r15d, esi
0047277d nop       dword ptr [rax]
00472780 cmp       r15d, dword ptr [r14 + 0x1d0]
00472787 jae       0x7ff7ea382eec
0047278d movsxd    rdi, r15d
00472790 imul      rdi, rdi, 0x58
00472794 add       rdi, qword ptr [r14 + 0x1d8]
0047279b lea       r13, [rdi + 8]
0047279f mov       rdx, r13
004727a2 lea       rcx, [rip + 0x1cc2367]
004727a9 call      0x7ff7e9f3bdff ; ?folderExists@UtilityT@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
004727ae test      al, al
004727b0 jne       0x7ff7ea382a98
004727b6 mov       r8, r13
004727b9 lea       rdx, [rip + 0x1264a70]
004727c0 lea       rcx, [rsp + 0x98]
004727c8 call      0x7ff7e9f3851f
004727cd nop
004727ce lea       r8, [rip + 0x1264a2b]
004727d5 mov       rdx, rax
004727d8 lea       rcx, [rsp + 0x70]
004727dd call      0x7ff7e9f42d6c
004727e2 mov       rdi, rax
004727e5 mov       ecx, dword ptr [rip + 0x1cbb429]
004727eb test      cl, 1
004727ee jne       0x7ff7ea382823
004727f0 or        ecx, 1
004727f3 mov       dword ptr [rip + 0x1cbb41b], ecx
004727f9 mov       ecx, 0x318
004727fe call      0x7ff7eade563a
00472803 mov       qword ptr [rsp + 0x30], rax
00472808 test      rax, rax
0047280b je        0x7ff7ea382817
0047280d mov       rcx, rax
00472810 call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00472815 jmp       0x7ff7ea38281a
00472817 mov       rax, rsi
0047281a mov       qword ptr [rip + 0x1cbb3e7], rax
00472821 jmp       0x7ff7ea38282a
00472823 mov       rax, qword ptr [rip + 0x1cbb3de]
0047282a mov       r8d, 4
00472830 mov       rdx, rdi
00472833 mov       rcx, rax
00472836 call      0x7ff7e9f1a68c ; ?log@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@Logger@@@Z
0047283b nop
0047283c cmp       qword ptr [rsp + 0x88], 0x10
00472845 jb        0x7ff7ea382851
00472847 mov       rcx, qword ptr [rsp + 0x70]
0047284c call      0x7ff7eade5628
00472851 mov       qword ptr [rsp + 0x88], 0xf
0047285d mov       qword ptr [rsp + 0x80], rsi
00472865 mov       byte ptr [rsp + 0x70], 0
0047286a cmp       qword ptr [rsp + 0xb0], 0x10
00472873 jb        0x7ff7ea382882
00472875 mov       rcx, qword ptr [rsp + 0x98]
0047287d call      0x7ff7eade5628
00472882 lea       rdx, [rip + 0x120baa7]
00472889 lea       rcx, [rsp + 0x250]
00472891 call      0x7ff7e9f210b8
00472896 nop
00472897 mov       rdx, rax
0047289a lea       rcx, [rsp + 0x2f0]
004728a2 call      0x7ff7e9f5855e
004728a7 nop
004728a8 mov       rdx, r13
004728ab mov       rcx, rax
004728ae call      0x7ff7e9f3b008
004728b3 lea       r8, [rip + 0x1cbf5fe]
004728ba lea       rdx, [rsp + 0x70]
004728bf mov       rcx, rax
004728c2 call      0x7ff7e9f1db57
004728c7 nop
004728c8 lea       rcx, [r14 + 0x1e8]
004728cf mov       rdx, rax
004728d2 call      0x7ff7e9f121a3
004728d7 nop
004728d8 cmp       qword ptr [rsp + 0x88], 0x10
004728e1 jb        0x7ff7ea3828ed
004728e3 mov       rcx, qword ptr [rsp + 0x70]
004728e8 call      0x7ff7eade5628
004728ed mov       qword ptr [rsp + 0x88], 0xf
004728f9 mov       qword ptr [rsp + 0x80], rsi
00472901 mov       byte ptr [rsp + 0x70], 0
00472906 cmp       qword ptr [rsp + 0x440], 0
0047290f je        0x7ff7ea38292c
00472911 lea       rcx, [rsp + 0x440]
00472919 call      qword ptr [rip + 0x1dd1cc1]
0047291f mov       rcx, qword ptr [rsp + 0x440]
00472927 call      0x7ff7eade5628
0047292c mov       qword ptr [rsp + 0x440], rsi
00472934 mov       qword ptr [rsp + 0x448], rsi
0047293c mov       qword ptr [rsp + 0x450], rsi
00472944 cmp       qword ptr [rsp + 0x3a0], 0x10
0047294d jb        0x7ff7ea38295c
0047294f mov       rcx, qword ptr [rsp + 0x388]
00472957 call      0x7ff7eade5628
0047295c mov       qword ptr [rsp + 0x3a0], 0xf
00472968 mov       qword ptr [rsp + 0x398], rsi
00472970 mov       byte ptr [rsp + 0x388], 0
00472978 cmp       qword ptr [rsp + 0x378], 0x10
00472981 jb        0x7ff7ea382990
00472983 mov       rcx, qword ptr [rsp + 0x360]
0047298b call      0x7ff7eade5628
00472990 mov       qword ptr [rsp + 0x378], 0xf
0047299c mov       qword ptr [rsp + 0x370], rsi
004729a4 mov       byte ptr [rsp + 0x360], 0
004729ac cmp       qword ptr [rsp + 0x350], 0x10
004729b5 jb        0x7ff7ea3829c4
004729b7 mov       rcx, qword ptr [rsp + 0x338]
004729bf call      0x7ff7eade5628
004729c4 mov       qword ptr [rsp + 0x350], 0xf
004729d0 mov       qword ptr [rsp + 0x348], rsi
004729d8 mov       byte ptr [rsp + 0x338], 0
004729e0 cmp       qword ptr [rsp + 0x328], 0x10
004729e9 jb        0x7ff7ea3829f8
004729eb mov       rcx, qword ptr [rsp + 0x310]
004729f3 call      0x7ff7eade5628
004729f8 mov       qword ptr [rsp + 0x328], 0xf
00472a04 mov       qword ptr [rsp + 0x320], rsi
00472a0c mov       byte ptr [rsp + 0x310], 0
00472a14 cmp       qword ptr [rsp + 0x2d8], 0x10
00472a1d jb        0x7ff7ea382a2c
00472a1f mov       rcx, qword ptr [rsp + 0x2c0]
00472a27 call      0x7ff7eade5628
00472a2c mov       qword ptr [rsp + 0x2d8], 0xf
00472a38 mov       qword ptr [rsp + 0x2d0], rsi
00472a40 mov       byte ptr [rsp + 0x2c0], 0
00472a48 cmp       qword ptr [rsp + 0x2b0], 0x10
00472a51 jb        0x7ff7ea382a60
00472a53 mov       rcx, qword ptr [rsp + 0x298]
00472a5b call      0x7ff7eade5628
00472a60 mov       qword ptr [rsp + 0x2b0], 0xf
00472a6c mov       qword ptr [rsp + 0x2a8], rsi
00472a74 mov       byte ptr [rsp + 0x298], 0
00472a7c cmp       qword ptr [rsp + 0x288], 0x10
00472a85 jb        0x7ff7ea382ee4
00472a8b mov       rcx, qword ptr [rsp + 0x270]
00472a93 jmp       0x7ff7ea382edf
00472a98 mov       ecx, dword ptr [rdi]
00472a9a test      ecx, ecx
00472a9c je        0x7ff7ea382cd9
00472aa2 dec       ecx
00472aa4 je        0x7ff7ea382abb
00472aa6 dec       ecx
00472aa8 jne       0x7ff7ea382ee4
00472aae mov       rcx, r13
00472ab1 call      0x7ff7e9f632ba
00472ab6 jmp       0x7ff7ea382ee4
00472abb mov       r8, r13
00472abe lea       rdx, [rip + 0x120803b]
00472ac5 lea       rcx, [rsp + 0x1b0]
00472acd call      0x7ff7e9f3851f
00472ad2 nop
00472ad3 lea       r8, [rip + 0x126470a]
00472ada mov       rdx, rax
00472add lea       rcx, [rsp + 0x188]
00472ae5 call      0x7ff7e9f42d6c
00472aea nop
00472aeb add       rdi, 0x30
00472aef mov       r8, rdi
00472af2 mov       rdx, rax
00472af5 lea       rcx, [rsp + 0x1d8]
00472afd call      0x7ff7e9f49c1b
00472b02 nop
00472b03 mov       r8d, 2
00472b09 mov       rdx, rax
00472b0c mov       rcx, r14
00472b0f call      0x7ff7e9f1a68c ; ?log@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@Logger@@@Z
00472b14 nop
00472b15 cmp       qword ptr [rsp + 0x1f0], 0x10
00472b1e jb        0x7ff7ea382b2d
00472b20 mov       rcx, qword ptr [rsp + 0x1d8]
00472b28 call      0x7ff7eade5628
00472b2d mov       qword ptr [rsp + 0x1f0], 0xf
00472b39 mov       qword ptr [rsp + 0x1e8], rsi
00472b41 mov       byte ptr [rsp + 0x1d8], 0
00472b49 cmp       qword ptr [rsp + 0x1a0], 0x10
00472b52 jb        0x7ff7ea382b61
00472b54 mov       rcx, qword ptr [rsp + 0x188]
00472b5c call      0x7ff7eade5628
00472b61 mov       qword ptr [rsp + 0x1a0], 0xf
00472b6d mov       qword ptr [rsp + 0x198], rsi
00472b75 mov       byte ptr [rsp + 0x188], 0
00472b7d cmp       qword ptr [rsp + 0x1c8], 0x10
00472b86 jb        0x7ff7ea382b95
00472b88 mov       rcx, qword ptr [rsp + 0x1b0]
00472b90 call      0x7ff7eade5628
00472b95 mov       qword ptr [rsp + 0x1c8], 0xf
00472ba1 mov       qword ptr [rsp + 0x1c0], rsi
00472ba9 mov       byte ptr [rsp + 0x1b0], 0
00472bb1 mov       qword ptr [rsp + 0x178], 7
00472bbd mov       qword ptr [rsp + 0x170], rsi
00472bc5 mov       word ptr [rsp + 0x160], si
00472bcd mov       r12, qword ptr [rdi + 0x10]
00472bd1 test      r12, r12
00472bd4 je        0x7ff7ea382c0e
00472bd6 mov       rax, qword ptr [rdi + 0x18]
00472bda cmp       rax, 0x10
00472bde jb        0x7ff7ea382be5
00472be0 mov       rbx, qword ptr [rdi]
00472be3 jmp       0x7ff7ea382be8
00472be5 mov       rbx, rdi
00472be8 cmp       rax, 0x10
00472bec jb        0x7ff7ea382bf1
00472bee mov       rdi, qword ptr [rdi]
00472bf1 call      0x7ff7eadefc90
00472bf6 lea       rdx, [r12 + rbx]
00472bfa mov       r9, rax
00472bfd lea       r8, [rsp + 0x160]
00472c05 mov       rcx, rdi
00472c08 call      0x7ff7eadef440
00472c0d nop
00472c0e mov       qword ptr [rsp + 0x128], 7
00472c1a mov       qword ptr [rsp + 0x120], rsi
00472c22 mov       word ptr [rsp + 0x110], si
00472c2a mov       rbx, qword ptr [r13 + 0x10]
00472c2e test      rbx, rbx
00472c31 je        0x7ff7ea382c6d
00472c33 mov       rax, qword ptr [r13 + 0x18]
00472c37 cmp       rax, 0x10
00472c3b jb        0x7ff7ea382c43
00472c3d mov       rdi, qword ptr [r13]
00472c41 jmp       0x7ff7ea382c46
00472c43 mov       rdi, r13
00472c46 cmp       rax, 0x10
00472c4a jb        0x7ff7ea382c50
00472c4c mov       r13, qword ptr [r13]
00472c50 call      0x7ff7eadefc90
00472c55 lea       rdx, [rdi + rbx]
00472c59 mov       r9, rax
00472c5c lea       r8, [rsp + 0x110]
00472c64 mov       rcx, r13
00472c67 call      0x7ff7eadef440
00472c6c nop
00472c6d mov       r8d, 1
00472c73 lea       rdx, [rsp + 0x160]
00472c7b lea       rcx, [rsp + 0x110]
00472c83 call      0x7ff7e9f4fa30
00472c88 nop
00472c89 cmp       qword ptr [rsp + 0x128], 8
00472c92 jb        0x7ff7ea382ca1
00472c94 mov       rcx, qword ptr [rsp + 0x110]
00472c9c call      0x7ff7eade5628
00472ca1 mov       qword ptr [rsp + 0x128], 7
00472cad mov       qword ptr [rsp + 0x120], rsi
00472cb5 mov       word ptr [rsp + 0x110], si
00472cbd cmp       qword ptr [rsp + 0x178], 8
00472cc6 jb        0x7ff7ea382ee4
00472ccc mov       rcx, qword ptr [rsp + 0x160]
00472cd4 jmp       0x7ff7ea382edf
00472cd9 mov       r8, r13
00472cdc lea       rdx, [rip + 0x1207e1d]
00472ce3 lea       rcx, [rsp + 0x98]
00472ceb call      0x7ff7e9f3851f
00472cf0 nop
00472cf1 lea       r8, [rip + 0x12644ec]
00472cf8 mov       rdx, rax
00472cfb lea       rcx, [rsp + 0xe8]
00472d03 call      0x7ff7e9f42d6c
00472d08 nop
00472d09 add       rdi, 0x30
00472d0d mov       r8, rdi
00472d10 mov       rdx, rax
00472d13 lea       rcx, [rsp + 0x200]
00472d1b call      0x7ff7e9f49c1b
00472d20 nop
00472d21 mov       r8d, 2
00472d27 mov       rdx, rax
00472d2a mov       rcx, r14
00472d2d call      0x7ff7e9f1a68c ; ?log@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@Logger@@@Z
00472d32 nop
00472d33 cmp       qword ptr [rsp + 0x218], 0x10
00472d3c jb        0x7ff7ea382d4b
00472d3e mov       rcx, qword ptr [rsp + 0x200]
00472d46 call      0x7ff7eade5628
00472d4b mov       qword ptr [rsp + 0x218], 0xf
00472d57 mov       qword ptr [rsp + 0x210], rsi
00472d5f mov       byte ptr [rsp + 0x200], 0
00472d67 cmp       qword ptr [rsp + 0x100], 0x10
00472d70 jb        0x7ff7ea382d7f
00472d72 mov       rcx, qword ptr [rsp + 0xe8]
00472d7a call      0x7ff7eade5628
00472d7f mov       qword ptr [rsp + 0x100], 0xf
00472d8b mov       qword ptr [rsp + 0xf8], rsi
00472d93 mov       byte ptr [rsp + 0xe8], 0
00472d9b cmp       qword ptr [rsp + 0xb0], 0x10
00472da4 jb        0x7ff7ea382db3
00472da6 mov       rcx, qword ptr [rsp + 0x98]
00472dae call      0x7ff7eade5628
00472db3 mov       qword ptr [rsp + 0xb0], 0xf
00472dbf mov       qword ptr [rsp + 0xa8], rsi
00472dc7 mov       byte ptr [rsp + 0x98], 0
00472dcf mov       qword ptr [rsp + 0x88], 7
00472ddb mov       qword ptr [rsp + 0x80], rsi
00472de3 mov       word ptr [rsp + 0x70], si
00472de8 mov       r12, qword ptr [rdi + 0x10]
00472dec test      r12, r12
00472def je        0x7ff7ea382e26
00472df1 mov       rax, qword ptr [rdi + 0x18]
00472df5 cmp       rax, 0x10
00472df9 jb        0x7ff7ea382e00
00472dfb mov       rbx, qword ptr [rdi]
00472dfe jmp       0x7ff7ea382e03
00472e00 mov       rbx, rdi
00472e03 cmp       rax, 0x10
00472e07 jb        0x7ff7ea382e0c
00472e09 mov       rdi, qword ptr [rdi]
00472e0c call      0x7ff7eadefc90
00472e11 lea       rdx, [rbx + r12]
00472e15 mov       r9, rax
00472e18 lea       r8, [rsp + 0x70]
00472e1d mov       rcx, rdi
00472e20 call      0x7ff7eadef440
00472e25 nop
00472e26 mov       qword ptr [rsp + 0xd8], 7
00472e32 mov       qword ptr [rsp + 0xd0], rsi
00472e3a mov       word ptr [rsp + 0xc0], si
00472e42 mov       rbx, qword ptr [r13 + 0x10]
00472e46 test      rbx, rbx
00472e49 je        0x7ff7ea382e85
00472e4b mov       rax, qword ptr [r13 + 0x18]
00472e4f cmp       rax, 0x10
00472e53 jb        0x7ff7ea382e5b
00472e55 mov       rdi, qword ptr [r13]
00472e59 jmp       0x7ff7ea382e5e
00472e5b mov       rdi, r13
00472e5e cmp       rax, 0x10
00472e62 jb        0x7ff7ea382e68
00472e64 mov       r13, qword ptr [r13]
00472e68 call      0x7ff7eadefc90
00472e6d lea       rdx, [rdi + rbx]
00472e71 mov       r9, rax
00472e74 lea       r8, [rsp + 0xc0]
00472e7c mov       rcx, r13
00472e7f call      0x7ff7eadef440
00472e84 nop
00472e85 xor       r8d, r8d
00472e88 lea       rdx, [rsp + 0x70]
00472e8d lea       rcx, [rsp + 0xc0]
00472e95 call      0x7ff7e9f4fa30
00472e9a nop
00472e9b cmp       qword ptr [rsp + 0xd8], 8
00472ea4 jb        0x7ff7ea382eb3
00472ea6 mov       rcx, qword ptr [rsp + 0xc0]
00472eae call      0x7ff7eade5628
00472eb3 mov       qword ptr [rsp + 0xd8], 7
00472ebf mov       qword ptr [rsp + 0xd0], rsi
00472ec7 mov       word ptr [rsp + 0xc0], si
00472ecf cmp       qword ptr [rsp + 0x88], 8
00472ed8 jb        0x7ff7ea382ee4
00472eda mov       rcx, qword ptr [rsp + 0x70]
00472edf call      0x7ff7eade5628
00472ee4 inc       r15d
00472ee7 jmp       0x7ff7ea382780
00472eec mov       rdx, qword ptr [r14 + 0x1d8]
00472ef3 add       rdx, 0x30
00472ef7 lea       r8, [rip + 0x1227a4e]
00472efe lea       rcx, [rsp + 0x228]
00472f06 call      0x7ff7e9f32c4b
00472f0b nop
00472f0c lea       rdi, [rsp + 0x30]
00472f11 cmp       qword ptr [r14 + 0xd0], 0
00472f19 je        0x7ff7ea382f34
00472f1b mov       rcx, qword ptr [r14 + 0xc8]
00472f22 mov       rax, qword ptr [r14 + 0xe8]
00472f29 mov       rax, qword ptr [rax + rcx*8]
00472f2d mov       qword ptr [rsp + 0x30], rax
00472f32 jmp       0x7ff7ea382f39
00472f34 mov       qword ptr [rsp + 0x30], rsi
00472f39 mov       rdi, qword ptr [rdi]
00472f3c mov       qword ptr [rsp + 0x30], rdi
00472f41 test      rdi, rdi
00472f44 je        0x7ff7ea383036
00472f4a lea       r8, [rdi + 0x10]
00472f4e lea       rdx, [rsp + 0x228]
00472f56 lea       rcx, [rsp + 0x98]
00472f5e call      0x7ff7e9f36945
00472f63 mov       rbx, rax
00472f66 mov       qword ptr [rsp + 0xd8], 7
00472f72 mov       qword ptr [rsp + 0xd0], rsi
00472f7a mov       word ptr [rsp + 0xc0], si
00472f82 mov       r13, qword ptr [rax + 0x10]
00472f86 test      r13, r13
00472f89 je        0x7ff7ea382fc3
00472f8b mov       rax, qword ptr [rax + 0x18]
00472f8f cmp       rax, 0x10
00472f93 jb        0x7ff7ea382f9a
00472f95 mov       r12, qword ptr [rbx]
00472f98 jmp       0x7ff7ea382f9d
00472f9a mov       r12, rbx
00472f9d cmp       rax, 0x10
00472fa1 jb        0x7ff7ea382fa6
00472fa3 mov       rbx, qword ptr [rbx]
00472fa6 call      0x7ff7eadefc90
00472fab lea       rdx, [r12 + r13]
00472faf mov       r9, rax
00472fb2 lea       r8, [rsp + 0xc0]
00472fba mov       rcx, rbx
00472fbd call      0x7ff7eadef440
00472fc2 nop
00472fc3 xor       edx, edx
00472fc5 lea       rcx, [rsp + 0xc0]
00472fcd call      0x7ff7eadf77c0
00472fd2 nop
00472fd3 cmp       qword ptr [rsp + 0xd8], 8
00472fdc jb        0x7ff7ea382feb
00472fde mov       rcx, qword ptr [rsp + 0xc0]
00472fe6 call      0x7ff7eade5628
00472feb mov       qword ptr [rsp + 0xd8], 7
00472ff7 mov       qword ptr [rsp + 0xd0], rsi
00472fff mov       word ptr [rsp + 0xc0], si
00473007 cmp       qword ptr [rsp + 0xb0], 0x10
00473010 jb        0x7ff7ea383020
00473012 mov       rcx, qword ptr [rsp + 0x98]
0047301a call      0x7ff7eade5628
0047301f nop
00473020 jmp       0x7ff7ea382f39
00473025 xor       esi, esi
00473027 mov       rdi, qword ptr [rsp + 0x30]
0047302c mov       r14, qword ptr [rsp + 0x38]
00473031 jmp       0x7ff7ea382f39
00473036 lea       rdi, [rip + 0x1236c93]
0047303d mov       qword ptr [rsp + 0x40], rdi
00473042 mov       rax, qword ptr [rip + 0x1cc090f]
00473049 mov       qword ptr [rsp + 0x48], rax
0047304e lea       rcx, [rsp + 0x40]
00473053 call      qword ptr [rip + 0x1236c7f]
00473059 cmp       qword ptr [rsp + 0x48], 0
0047305f jg        0x7ff7ea383070
00473061 lea       rcx, [rsp + 0x40]
00473066 mov       rax, qword ptr [rsp + 0x40]
0047306b call      qword ptr [rax + 0x10]
0047306e jmp       0x7ff7ea3830ad
00473070 lea       rcx, [rsp + 0x30]
00473075 call      qword ptr [rip + 0x1dd09f5]
0047307b mov       rdx, qword ptr [rsp + 0x48]
00473080 sub       rdx, qword ptr [rsp + 0x30]
00473085 mov       rax, qword ptr [rip + 0x1cbef4c]
0047308c cmp       rdx, rax
0047308f jge       0x7ff7ea38309b
00473091 sub       rdx, rax
00473094 mov       qword ptr [rsp + 0x48], rdx
00473099 jmp       0x7ff7ea3830a0
0047309b mov       qword ptr [rsp + 0x48], rsi
004730a0 lea       rcx, [rsp + 0x40]
004730a5 mov       rax, qword ptr [rsp + 0x40]
004730aa call      qword ptr [rax + 0x10]
004730ad mov       rax, qword ptr [rsp + 0x48]
004730b2 neg       rax
004730b5 pxor      xmm1, xmm1
004730b9 cvtsi2sd  xmm1, rax
004730be mulsd     xmm1, qword ptr [rip + 0x1236c5a]
004730c6 pxor      xmm0, xmm0
004730ca cvtsi2sd  xmm0, qword ptr [rip + 0x1cbeefd]
004730d3 divsd     xmm1, xmm0
004730d7 mov       qword ptr [rsp + 0x40], rdi
004730dc mulsd     xmm1, qword ptr [rip + 0x1236c4c]
004730e4 subsd     xmm1, qword ptr [rsp + 0x50]
004730ea mov       edi, 6
004730ef mulsd     xmm1, qword ptr [rip + 0x1244b29]
004730f7 addsd     xmm1, qword ptr [rip + 0x1210341]
004730ff movapd    xmm0, xmm1
00473103 call      0x7ff7eade5c3c
00473108 movapd    xmm1, xmm0
0047310c mov       dword ptr [rsp + 0x28], esi
00473110 mov       byte ptr [rsp + 0x20], 0x20
00473115 movzx     r9d, si
00473119 movzx     r8d, di
0047311d lea       rcx, [rsp + 0xe8]
00473125 call      qword ptr [rip + 0x1dd4e4d]
0047312b nop
0047312c mov       r8, rax
0047312f lea       rdx, [rip + 0x126408a]
00473136 lea       rcx, [rsp + 0x98]
0047313e call      0x7ff7e9f623dd
00473143 nop
00473144 lea       r8, [rip + 0x12454d5]
0047314b mov       rdx, rax
0047314e lea       rcx, [rsp + 0x138]
00473156 call      0x7ff7e9f42d6c
0047315b nop
0047315c cmp       qword ptr [rsp + 0xb0], 0x10
00473165 jb        0x7ff7ea383174
00473167 mov       rcx, qword ptr [rsp + 0x98]
0047316f call      0x7ff7eade5628
00473174 mov       qword ptr [rsp + 0xb0], 0xf
00473180 mov       qword ptr [rsp + 0xa8], rsi
00473188 mov       byte ptr [rsp + 0x98], 0
00473190 cmp       qword ptr [rsp + 0x100], 0x10
00473199 jb        0x7ff7ea3831a8
0047319b mov       rcx, qword ptr [rsp + 0xe8]
004731a3 call      0x7ff7eade5628
004731a8 mov       qword ptr [rsp + 0x100], 0xf
004731b4 mov       qword ptr [rsp + 0xf8], rsi
004731bc mov       byte ptr [rsp + 0xe8], 0
004731c4 mov       r8d, 2
004731ca lea       rdx, [rsp + 0x138]
004731d2 mov       rcx, r14
004731d5 call      0x7ff7e9f1a68c ; ?log@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@Logger@@@Z
004731da lea       rdx, [rsp + 0x138]
004731e2 mov       ecx, 0xffff80
004731e7 call      0x7ff7e9f41a3e
004731ec mov       dword ptr [r14 + 0x1e0], 2
004731f7 cmp       qword ptr [rsp + 0x150], 0x10
00473200 jb        0x7ff7ea38320f
00473202 mov       rcx, qword ptr [rsp + 0x138]
0047320a call      0x7ff7eade5628
0047320f mov       qword ptr [rsp + 0x150], 0xf
0047321b mov       qword ptr [rsp + 0x148], rsi
00473223 mov       byte ptr [rsp + 0x138], 0
0047322b cmp       qword ptr [rsp + 0x240], 0x10
00473234 jb        0x7ff7ea383244
00473236 mov       rcx, qword ptr [rsp + 0x228]
0047323e call      0x7ff7eade5628
00473243 nop
00473244 jmp       0x7ff7ea38324b
00473246 mov       r14, qword ptr [rsp + 0x38]
0047324b cmp       byte ptr [r14 + 0x45], 0
00473250 jne       0x7ff7ea3832a0
00473252 call      qword ptr [rip + 0x1dd10e0]
00473258 mov       qword ptr [rsp + 0x50], rax
0047325d call      qword ptr [rip + 0x1dd119d]
00473263 mov       dword ptr [rsp + 0x38], eax
00473267 lea       r8, [rsp + 0x50]
0047326c lea       rdx, [rsp + 0x38]
00473271 lea       rcx, [rsp + 0x30]
00473276 call      0x7ff7e9f4b2eb
0047327b lea       rdx, [rsp + 0x30]
00473280 lea       rcx, [r14 + 0x48]
00473284 call      0x7ff7e9f5f8ae
00473289 mov       byte ptr [r14 + 0x14], 0
0047328e lea       rcx, [r14 + 0x68]
00473292 call      0x7ff7e9f3e843
00473297 lea       rcx, [r14 + 0x48]
0047329b call      0x7ff7e9f3e843
004732a0 xor       eax, eax
004732a2 mov       rcx, qword ptr [rsp + 0x460]
004732aa xor       rcx, rsp
004732ad call      0x7ff7eade5db0
004732b2 lea       r11, [rsp + 0x470]
004732ba mov       rbx, qword ptr [r11 + 0x38]
004732be mov       rsi, qword ptr [r11 + 0x40]
004732c2 mov       rsp, r11
004732c5 pop       r15
004732c7 pop       r14
004732c9 pop       r13
004732cb pop       r12
004732cd pop       rdi
004732ce ret
004732cf int3
