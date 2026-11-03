<unexported runtime function> RVA=82aaf0 slot=None PID=9388
0082aaf0 mov       rax, rsp
0082aaf3 push      rbp
0082aaf4 push      r12
0082aaf6 push      r13
0082aaf8 push      r14
0082aafa push      r15
0082aafc lea       rbp, [rax - 0x1c8]
0082ab03 sub       rsp, 0x2a0
0082ab0a mov       qword ptr [rbp - 0x80], 0xfffffffffffffffe
0082ab12 mov       qword ptr [rax + 0x10], rbx
0082ab16 mov       qword ptr [rax + 0x18], rsi
0082ab1a mov       qword ptr [rax + 0x20], rdi
0082ab1e mov       rax, qword ptr [rip + 0x18eb893]
0082ab25 xor       rax, rsp
0082ab28 mov       qword ptr [rbp + 0x190], rax
0082ab2f mov       rsi, rcx
0082ab32 mov       qword ptr [rsp + 0x40], rcx
0082ab37 cmp       qword ptr [rcx + 0x70], 0
0082ab3c jne       0x7ff7ea73b4ed
0082ab42 mov       edi, 1
0082ab47 cmp       byte ptr [rip + 0x1907934], 0
0082ab4e je        0x7ff7ea73ab58
0082ab50 mov       ecx, edi
0082ab52 call      qword ptr [rip + 0x1a1e6c8]
0082ab58 cmp       byte ptr [rip + 0x19078a9], 0
0082ab5f je        0x7ff7ea73ac24
0082ab65 call      0x7ff7e9f5b722 ; ?getSingleton@SaveFileSystem@@SAPEAV1@XZ
0082ab6a cmp       dword ptr [rax + 0x1e0], 0
0082ab71 jg        0x7ff7ea73ac24
0082ab77 lea       rcx, [rip + 0x1908532]
0082ab7e call      0x7ff7e9f404d6 ; ?allThreadQueuesAreClear@GameWorld@@QEAA_NXZ
0082ab83 test      al, al
0082ab85 je        0x7ff7ea73ac53
0082ab8b mov       rcx, qword ptr [rip + 0x1908cae]
0082ab92 test      rcx, rcx
0082ab95 je        0x7ff7ea73ab9c
0082ab97 call      0x7ff7e9f3f58b ; ?waitForRunningStop_Blocking@ThreadClass@@QEAA_NXZ
0082ab9c call      0x7ff7e9f47952
0082aba1 test      rax, rax
0082aba4 je        0x7ff7ea73abb3
0082aba6 call      0x7ff7e9f47952
0082abab mov       rcx, rax
0082abae call      0x7ff7e9f3f58b ; ?waitForRunningStop_Blocking@ThreadClass@@QEAA_NXZ
0082abb3 mov       rcx, qword ptr [rip + 0x190850e]
0082abba test      rcx, rcx
0082abbd je        0x7ff7ea73abc4
0082abbf call      0x7ff7e9f3f58b ; ?waitForRunningStop_Blocking@ThreadClass@@QEAA_NXZ
0082abc4 lea       rcx, [rip + 0x19084e5]
0082abcb call      0x7ff7e9f52a05 ; ?flushKillList@GameWorld@@QEAAXXZ
0082abd0 mov       rcx, qword ptr [rip + 0x1908a59]
0082abd7 cmp       qword ptr [rcx + 0x298], 0
0082abdf je        0x7ff7ea73abe6
0082abe1 call      0x7ff7e9f4fada ; ?toggleLevelEditorOnSelectedTown@PlayerInterface@@QEAAXXZ
0082abe6 mov       rcx, qword ptr [rip + 0x1908973]
0082abed call      0x7ff7e9f3455f ; ?shutdown@NavMesh@@QEAAXXZ
0082abf2 mov       rcx, qword ptr [rip + 0x1908d77]
0082abf9 test      rcx, rcx
0082abfc je        0x7ff7ea73ac05
0082abfe mov       rax, qword ptr [rcx]
0082ac01 mov       edx, edi
0082ac03 call      qword ptr [rax]
0082ac05 xor       edi, edi
0082ac07 mov       qword ptr [rip + 0x1908d62], rdi
0082ac0e call      0x7ff7e9f5b722 ; ?getSingleton@SaveFileSystem@@SAPEAV1@XZ
0082ac13 mov       rcx, rax
0082ac16 mov       dl, 1
0082ac18 call      0x7ff7e9f57000 ; ?cleanup@SaveFileSystem@@QEAAX_N@Z
0082ac1d xor       al, al
0082ac1f jmp       0x7ff7ea73b4ef
0082ac24 cmp       qword ptr [rip + 0x1908c14], 0
0082ac2c je        0x7ff7ea73ac53
0082ac2e lea       rcx, [rip + 0x190780b]
0082ac35 call      0x7ff7e9f4b4c6 ; ?characterMultithreadingEnabled@OptionsHolder@@QEAA_NXZ
0082ac3a test      al, al
0082ac3c jne       0x7ff7ea73ac53
0082ac3e cmp       byte ptr [rsi + 0x88], al
0082ac44 jne       0x7ff7ea73ac53
0082ac46 mov       rcx, qword ptr [rip + 0x1908bf3]
0082ac4d mov       rax, qword ptr [rcx]
0082ac50 call      qword ptr [rax + 0x18]
0082ac53 mov       byte ptr [rsi + 0x88], 0
0082ac5a mov       rcx, qword ptr [rip + 0x1908d0f]
0082ac61 mov       rax, qword ptr [rcx]
0082ac64 call      qword ptr [rax + 0x20]
0082ac67 cmp       qword ptr [rip + 0x1907661], 0
0082ac6f je        0x7ff7ea73ac87
0082ac71 call      0x7ff7e9f42c4f ; ?getSingleton@SaveManager@@SAPEAV1@XZ
0082ac76 mov       rcx, rax
0082ac79 call      0x7ff7e9f27607 ; ?execute@SaveManager@@QEAAXXZ
0082ac7e movzx     eax, dil
0082ac82 jmp       0x7ff7ea73b4ef
0082ac87 mov       rax, qword ptr [rip + 0x190762a]
0082ac8e mov       rcx, qword ptr [rax + 0x20]
0082ac92 test      rcx, rcx
0082ac95 je        0x7ff7ea73ac9c
0082ac97 call      0x7ff7e9f540b2
0082ac9c movss     xmm1, dword ptr [rip + 0x1907a90]
0082aca4 lea       rcx, [rip + 0x1908405]
0082acab call      0x7ff7e9f3671a ; ?_NV_mainLoop_GPUSensitiveStuff@GameWorld@@QEAAXM@Z | ?mainLoop_GPUSensitiveStuff@GameWorld@@UEAAXM@Z
0082acb0 lea       rcx, [rip + 0x1909e59]
0082acb7 call      0x7ff7e9f33173 ; ?flushMarkerQueue@UtilityT@@QEAAXXZ
0082acbc lea       rdx, [rsp + 0x30]
0082acc1 mov       rcx, qword ptr [rip + 0x19075f8]
0082acc8 mov       rcx, qword ptr [rcx + 0x58]
0082accc call      qword ptr [rip + 0x1a1c49e]
0082acd2 mov       r11, qword ptr [rip + 0x19075df]
0082acd9 mov       rbx, qword ptr [r11 + 0x60]
0082acdd lea       rdx, [rsp + 0x48]
0082ace2 mov       rcx, rbx
0082ace5 call      qword ptr [rip + 0x1a1c40d]
0082aceb movss     xmm1, dword ptr [rsp + 0x30]
0082acf1 subss     xmm1, dword ptr [rax]
0082acf5 movss     xmm0, dword ptr [rsp + 0x38]
0082acfb subss     xmm0, dword ptr [rax + 8]
0082ad00 mulss     xmm0, xmm0
0082ad04 mulss     xmm1, xmm1
0082ad08 addss     xmm0, xmm1
0082ad0c comiss    xmm0, dword ptr [rip + 0xe5cb9d]
0082ad13 jbe       0x7ff7ea73ad80
0082ad15 xorps     xmm0, xmm0
0082ad18 movss     dword ptr [rsp + 0x34], xmm0
0082ad1e lea       rdx, [rbp - 0x50]
0082ad22 mov       rcx, rbx
0082ad25 call      qword ptr [rip + 0x1a1c3cd]
0082ad2b mov       r8, rax
0082ad2e lea       rdx, [rsp + 0x48]
0082ad33 lea       rcx, [rsp + 0x30]
0082ad38 call      qword ptr [rip + 0x1a1c112]
0082ad3e mov       r11, qword ptr [rbx]
0082ad41 xor       r8d, r8d
0082ad44 lea       rdx, [rsp + 0x30]
0082ad49 mov       rcx, rbx
0082ad4c call      qword ptr [r11 + 0x690]
0082ad53 call      0x7ff7e9f17676
0082ad58 mov       rcx, rax
0082ad5b lea       rdx, [rsp + 0x48]
0082ad60 call      0x7ff7e9f205eb
0082ad65 lea       rdx, [rsp + 0x48]
0082ad6a mov       rcx, qword ptr [rip + 0x1908bef]
0082ad71 call      0x7ff7e9f4dfa5 ; ?shiftGroundEffects@ZoneManager@@QEAAXAEBVVector3@Ogre@@@Z
0082ad76 lea       rcx, [rsp + 0x48]
0082ad7b call      0x7ff7e9f63c15 ; ?shiftEffects@CombatClass@@SAXAEBVVector3@Ogre@@@Z
0082ad80 xor       al, al
0082ad82 xor       cl, cl
0082ad84 mov       dword ptr [rip + 0x1907689], 0
0082ad8e cmp       byte ptr [rip + 0x190767f], al
0082ad94 je        0x7ff7ea73ada7
0082ad96 cmp       byte ptr [rip + 0x1907679], cl
0082ad9c jne       0x7ff7ea73adba
0082ad9e mov       byte ptr [rip + 0x1907672], dil
0082ada5 jmp       0x7ff7ea73adba
0082ada7 movzx     eax, al
0082adaa cmp       byte ptr [rip + 0x1907664], 0
0082adb1 cmovne    eax, edi
0082adb4 mov       byte ptr [rip + 0x190765f], al
0082adba cmp       byte ptr [rip + 0x1907653], 0
0082adc1 je        0x7ff7ea73add5
0082adc3 cmp       byte ptr [rip + 0x190764c], 0
0082adca jne       0x7ff7ea73ade8
0082adcc mov       byte ptr [rip + 0x1907645], dil
0082add3 jmp       0x7ff7ea73ade8
0082add5 movzx     eax, cl
0082add8 cmp       byte ptr [rip + 0x1907637], 0
0082addf cmovne    eax, edi
0082ade2 mov       byte ptr [rip + 0x1907632], al
0082ade8 mov       rcx, qword ptr [rip + 0x19075b1]
0082adef mov       rbx, qword ptr [rcx]
0082adf2 cmp       rbx, rcx
0082adf5 je        0x7ff7ea73b487
0082adfb xor       edi, edi
0082adfd nop       dword ptr [rax]
0082ae00 mov       rax, qword ptr [rbx + 0x18]
0082ae04 cmp       dword ptr [rax + 8], 0x10
0082ae08 jne       0x7ff7ea73b42c
0082ae0e mov       qword ptr [rbp - 0x60], 0xf
0082ae16 mov       qword ptr [rbp - 0x68], rdi
0082ae1a mov       byte ptr [rbp - 0x78], 0
0082ae1e mov       r8d, 0xb
0082ae24 lea       rdx, [rip + 0xef09b5]
0082ae2b lea       rcx, [rbp - 0x78]
0082ae2f call      0x7ff7e9f5d7c5
0082ae34 lea       rdx, [rbp - 0x78]
0082ae38 lea       rcx, [rbp + 0x150]
0082ae3f call      0x7ff7e9f14d40
0082ae44 nop
0082ae45 mov       qword ptr [rbp + 0x78], 0xf
0082ae4d mov       qword ptr [rbp + 0x70], rdi
0082ae51 mov       byte ptr [rbp + 0x60], 0
0082ae55 mov       r8d, 5
0082ae5b lea       rdx, [rip + 0xe73442]
0082ae62 lea       rcx, [rbp + 0x60]
0082ae66 call      0x7ff7e9f5d7c5
0082ae6b nop
0082ae6c lea       r8, [rbp + 0x60]
0082ae70 lea       rdx, [rbp - 0x18]
0082ae74 lea       rcx, [rbp + 0x150]
0082ae7b call      0x7ff7e9f5c9b5
0082ae80 cmp       qword ptr [rbp - 8], 0
0082ae85 jne       0x7ff7ea73ae9b
0082ae87 cmp       qword ptr [rbp], 0x10
0082ae8c jb        0x7ff7ea73ae97
0082ae8e mov       rcx, qword ptr [rbp - 0x18]
0082ae92 call      0x7ff7eade5628
0082ae97 mov       esi, edi
0082ae99 jmp       0x7ff7ea73aec1
0082ae9b lea       rcx, [rbp - 0x18]
0082ae9f cmp       qword ptr [rbp], 0x10
0082aea4 cmovae    rcx, qword ptr [rbp - 0x18]
0082aea9 call      qword ptr [rip + 0x1a1a029]
0082aeaf mov       esi, eax
0082aeb1 cmp       qword ptr [rbp], 0x10
0082aeb6 jb        0x7ff7ea73aec1
0082aeb8 mov       rcx, qword ptr [rbp - 0x18]
0082aebc call      0x7ff7eade5628
0082aec1 lea       r15d, [rsi + 1]
0082aec5 cmp       qword ptr [rbp + 0x78], 0x10
0082aeca jb        0x7ff7ea73aed5
0082aecc mov       rcx, qword ptr [rbp + 0x60]
0082aed0 call      0x7ff7eade5628
0082aed5 mov       qword ptr [rbp + 0x78], 0xf
0082aedd mov       qword ptr [rbp + 0x70], rdi
0082aee1 mov       byte ptr [rbp + 0x60], 0
0082aee5 mov       qword ptr [rbp + 0xa0], 0xf
0082aef0 mov       qword ptr [rbp + 0x98], rdi
0082aef7 mov       byte ptr [rbp + 0x88], 0
0082aefe mov       r8d, 5
0082af04 lea       rdx, [rip + 0xe73399]
0082af0b lea       rcx, [rbp + 0x88]
0082af12 call      0x7ff7e9f5d7c5
0082af17 nop
0082af18 xor       r8d, r8d
0082af1b mov       dword ptr [rsp + 0x20], edi
0082af1f mov       r9b, 0x20
0082af22 mov       edx, r15d
0082af25 lea       rcx, [rbp - 0x18]
0082af29 call      qword ptr [rip + 0x1a1b1b1]
0082af2f nop
0082af30 mov       r8, rax
0082af33 lea       rdx, [rbp + 0x88]
0082af3a lea       rcx, [rbp + 0x150]
0082af41 call      0x7ff7e9f57e60
0082af46 nop
0082af47 cmp       qword ptr [rbp], 0x10
0082af4c jb        0x7ff7ea73af58
0082af4e mov       rcx, qword ptr [rbp - 0x18]
0082af52 call      0x7ff7eade5628
0082af57 nop
0082af58 cmp       qword ptr [rbp + 0xa0], 0x10
0082af60 jb        0x7ff7ea73af6e
0082af62 mov       rcx, qword ptr [rbp + 0x88]
0082af69 call      0x7ff7eade5628
0082af6e mov       qword ptr [rbp + 0xa0], 0xf
0082af79 mov       qword ptr [rbp + 0x98], rdi
0082af80 mov       byte ptr [rbp + 0x88], 0
0082af87 mov       qword ptr [rbp + 0x50], 0xf
0082af8f mov       qword ptr [rbp + 0x48], rdi
0082af93 mov       byte ptr [rbp + 0x38], 0
0082af97 mov       r8d, 0xa
0082af9d lea       rdx, [rip + 0xef082c]
0082afa4 lea       rcx, [rbp + 0x38]
0082afa8 call      0x7ff7e9f5d7c5
0082afad nop
0082afae lea       r8, [rbp + 0x38]
0082afb2 lea       rdx, [rbp - 0x18]
0082afb6 lea       rcx, [rbp + 0x150]
0082afbd call      0x7ff7e9f5c9b5
0082afc2 cmp       qword ptr [rbp - 8], 0
0082afc7 jne       0x7ff7ea73afde
0082afc9 cmp       qword ptr [rbp], 0x10
0082afce jb        0x7ff7ea73afd9
0082afd0 mov       rcx, qword ptr [rbp - 0x18]
0082afd4 call      0x7ff7eade5628
0082afd9 mov       r14d, edi
0082afdc jmp       0x7ff7ea73b006
0082afde lea       rcx, [rbp - 0x18]
0082afe2 cmp       qword ptr [rbp], 0x10
0082afe7 cmovae    rcx, qword ptr [rbp - 0x18]
0082afec call      qword ptr [rip + 0x1a19ee6]
0082aff2 mov       r14d, eax
0082aff5 cmp       qword ptr [rbp], 0x10
0082affa jb        0x7ff7ea73b006
0082affc mov       rcx, qword ptr [rbp - 0x18]
0082b000 call      0x7ff7eade5628
0082b005 nop
0082b006 cmp       qword ptr [rbp + 0x50], 0x10
0082b00b jb        0x7ff7ea73b016
0082b00d mov       rcx, qword ptr [rbp + 0x38]
0082b011 call      0x7ff7eade5628
0082b016 mov       qword ptr [rbp + 0x50], 0xf
0082b01e mov       qword ptr [rbp + 0x48], rdi
0082b022 mov       byte ptr [rbp + 0x38], 0
0082b026 mov       qword ptr [rbp + 0xc8], 0xf
0082b031 mov       qword ptr [rbp + 0xc0], rdi
0082b038 mov       byte ptr [rbp + 0xb0], 0
0082b03f mov       r8d, 6
0082b045 lea       rdx, [rip + 0xef0778]
0082b04c lea       rcx, [rbp + 0xb0]
0082b053 call      0x7ff7e9f5d7c5
0082b058 nop
0082b059 lea       r8, [rbp + 0xb0]
0082b060 lea       rdx, [rbp + 0x10]
0082b064 lea       rcx, [rbp + 0x150]
0082b06b call      0x7ff7e9f5c9b5
0082b070 nop
0082b071 cmp       qword ptr [rbp + 0xc8], 0x10
0082b079 jb        0x7ff7ea73b087
0082b07b mov       rcx, qword ptr [rbp + 0xb0]
0082b082 call      0x7ff7eade5628
0082b087 mov       qword ptr [rbp + 0xc8], 0xf
0082b092 mov       qword ptr [rbp + 0xc0], rdi
0082b099 mov       byte ptr [rbp + 0xb0], 0
0082b0a0 lea       rcx, [rbp + 0x10]
0082b0a4 mov       r12, qword ptr [rbp + 0x10]
0082b0a8 mov       r13, qword ptr [rbp + 0x28]
0082b0ac cmp       r13, 0x10
0082b0b0 cmovae    rcx, r12
0082b0b4 mov       r8d, 3
0082b0ba mov       rsi, qword ptr [rbp + 0x20]
0082b0be cmp       rsi, r8
0082b0c1 cmovb     r8, rsi
0082b0c5 lea       rdx, [rip + 0xef06f4]
0082b0cc call      0x7ff7eade5c12
0082b0d1 movsxd    rcx, eax
0082b0d4 test      eax, eax
0082b0d6 jne       0x7ff7ea73b0ee
0082b0d8 cmp       rsi, 3
0082b0dc jae       0x7ff7ea73b0e3
0082b0de or        ecx, 0xffffffff
0082b0e1 jmp       0x7ff7ea73b0ee
0082b0e3 mov       eax, edi
0082b0e5 cmp       rsi, 3
0082b0e9 setne     al
0082b0ec mov       ecx, eax
0082b0ee test      ecx, ecx
0082b0f0 sete      al
0082b0f3 test      al, al
0082b0f5 sete      al
0082b0f8 test      al, al
0082b0fa je        0x7ff7ea73b1b6
0082b100 lea       rcx, [rbp + 0x10]
0082b104 cmp       r13, 0x10
0082b108 cmovae    rcx, r12
0082b10c mov       r8d, 3
0082b112 cmp       rsi, r8
0082b115 cmovb     r8, rsi
0082b119 lea       rdx, [rip + 0xea4d84]
0082b120 call      0x7ff7eade5c12
0082b125 movsxd    rcx, eax
0082b128 test      eax, eax
0082b12a jne       0x7ff7ea73b142
0082b12c cmp       rsi, 3
0082b130 jae       0x7ff7ea73b137
0082b132 or        ecx, 0xffffffff
0082b135 jmp       0x7ff7ea73b142
0082b137 mov       eax, edi
0082b139 cmp       rsi, 3
0082b13d setne     al
0082b140 mov       ecx, eax
0082b142 test      ecx, ecx
0082b144 sete      al
0082b147 test      al, al
0082b149 sete      al
0082b14c test      al, al
0082b14e je        0x7ff7ea73b1b6
0082b150 lea       rcx, [rbp + 0x10]
0082b154 cmp       r13, 0x10
0082b158 cmovae    rcx, r12
0082b15c mov       r8d, 3
0082b162 cmp       rsi, r8
0082b165 cmovb     r8, rsi
0082b169 lea       rdx, [rip + 0xef064c]
0082b170 call      0x7ff7eade5c12
0082b175 movsxd    rcx, eax
0082b178 test      eax, eax
0082b17a jne       0x7ff7ea73b192
0082b17c cmp       rsi, 3
0082b180 jae       0x7ff7ea73b187
0082b182 or        ecx, 0xffffffff
0082b185 jmp       0x7ff7ea73b192
0082b187 mov       eax, edi
0082b189 cmp       rsi, 3
0082b18d setne     al
0082b190 mov       ecx, eax
0082b192 test      ecx, ecx
0082b194 sete      al
0082b197 test      al, al
0082b199 sete      al
0082b19c test      al, al
0082b19e je        0x7ff7ea73b1b6
0082b1a0 mov       r8d, 3
0082b1a6 lea       rdx, [rip + 0xef060f]
0082b1ad lea       rcx, [rbp + 0x10]
0082b1b1 call      0x7ff7e9f5d7c5
0082b1b6 mov       qword ptr [rbp + 0xf0], 0xf
0082b1c1 mov       qword ptr [rbp + 0xe8], rdi
0082b1c8 mov       byte ptr [rbp + 0xd8], 0
0082b1cf mov       r8d, 0xb
0082b1d5 lea       rdx, [rip + 0xef0604]
0082b1dc lea       rcx, [rbp + 0xd8]
0082b1e3 call      0x7ff7e9f5d7c5
0082b1e8 nop
0082b1e9 lea       rdx, [rbp + 0xd8]
0082b1f0 lea       rcx, [rbp + 0x150]
0082b1f7 call      0x7ff7e9f5fd40
0082b1fc nop
0082b1fd cmp       qword ptr [rbp + 0xf0], 0x10
0082b205 jb        0x7ff7ea73b213
0082b207 mov       rcx, qword ptr [rbp + 0xd8]
0082b20e call      0x7ff7eade5628
0082b213 mov       qword ptr [rbp + 0xf0], 0xf
0082b21e mov       qword ptr [rbp + 0xe8], rdi
0082b225 mov       byte ptr [rbp + 0xd8], 0
0082b22c cmp       r14d, 1
0082b230 jg        0x7ff7ea73b310
0082b236 mov       qword ptr [rbp], 0xf
0082b23e mov       qword ptr [rbp - 8], rdi
0082b242 mov       byte ptr [rbp - 0x18], 0
0082b246 mov       r8d, 0x14
0082b24c lea       rdx, [rip + 0xef054d]
0082b253 lea       rcx, [rbp - 0x18]
0082b257 call      0x7ff7e9f5d7c5
0082b25c nop
0082b25d xor       r8d, r8d
0082b260 mov       dword ptr [rsp + 0x20], edi
0082b264 mov       r9b, 0x20
0082b267 mov       edx, r15d
0082b26a lea       rcx, [rbp + 0x128]
0082b271 call      qword ptr [rip + 0x1a1ae69]
0082b277 nop
0082b278 or        r9, 0xffffffffffffffff
0082b27c xor       r8d, r8d
0082b27f mov       rdx, rax
0082b282 lea       rcx, [rbp - 0x18]
0082b286 call      0x7ff7e9f16708
0082b28b mov       r8d, 1
0082b291 lea       rdx, [rip + 0xe701b0]
0082b298 mov       rcx, rax
0082b29b call      0x7ff7e9f3c5cf
0082b2a0 or        r9, 0xffffffffffffffff
0082b2a4 xor       r8d, r8d
0082b2a7 lea       rdx, [rbp + 0x10]
0082b2ab mov       rcx, rax
0082b2ae call      0x7ff7e9f16708
0082b2b3 nop
0082b2b4 cmp       qword ptr [rbp + 0x140], 0x10
0082b2bc jb        0x7ff7ea73b2ca
0082b2be mov       rcx, qword ptr [rbp + 0x128]
0082b2c5 call      0x7ff7eade5628
0082b2ca mov       qword ptr [rbp + 0x140], 0xf
0082b2d5 mov       qword ptr [rbp + 0x138], rdi
0082b2dc mov       byte ptr [rbp + 0x128], 0
0082b2e3 lea       rdx, [rbp - 0x18]
0082b2e7 mov       rsi, qword ptr [rsp + 0x40]
0082b2ec mov       rcx, qword ptr [rsi + 0x28]
0082b2f0 call      qword ptr [rip + 0x1a1b742]
0082b2f6 nop
0082b2f7 cmp       qword ptr [rbp], 0x10
0082b2fc jb        0x7ff7ea73b3f9
0082b302 mov       rcx, qword ptr [rbp - 0x18]
0082b306 call      0x7ff7eade5628
0082b30b jmp       0x7ff7ea73b3f9
0082b310 mov       eax, dword ptr [rip + 0x190740a]
0082b316 test      al, 1
0082b318 jne       0x7ff7ea73b37c
0082b31a or        eax, 1
0082b31d mov       dword ptr [rip + 0x19073fd], eax
0082b323 mov       qword ptr [rsp + 0x70], 0xf
0082b32c mov       qword ptr [rsp + 0x68], rdi
0082b331 mov       byte ptr [rsp + 0x58], 0
0082b336 or        r9, 0xffffffffffffffff
0082b33a xor       r8d, r8d
0082b33d lea       rdx, [rbp + 0x10]
0082b341 lea       rcx, [rsp + 0x58]
0082b346 call      0x7ff7e9f3808d
0082b34b mov       byte ptr [rsp + 0x20], 1
0082b350 lea       r9, [rsp + 0x58]
0082b355 mov       r8d, r14d
0082b358 mov       rsi, qword ptr [rsp + 0x40]
0082b35d mov       rdx, qword ptr [rsi + 0x28]
0082b361 lea       rcx, [rip + 0x19072f8]
0082b368 call      0x7ff7e9f330a6
0082b36d lea       rcx, [rip - 0x812d5f]
0082b374 call      0x7ff7eade5e80
0082b379 nop
0082b37a jmp       0x7ff7ea73b381
0082b37c mov       rsi, qword ptr [rsp + 0x40]
0082b381 xor       r8d, r8d
0082b384 mov       dword ptr [rsp + 0x20], edi
0082b388 mov       r9b, 0x20
0082b38b mov       edx, r15d
0082b38e lea       rcx, [rbp + 0x100]
0082b395 call      qword ptr [rip + 0x1a1ad45]
0082b39b nop
0082b39c mov       r8, rax
0082b39f lea       rdx, [rip + 0xef03fa]
0082b3a6 lea       rcx, [rbp - 0x40]
0082b3aa call      0x7ff7e9f623dd
0082b3af mov       rcx, qword ptr [rip + 0x1906f02]
0082b3b6 mov       r8, rax
0082b3b9 mov       rdx, qword ptr [rcx + 0x58]
0082b3bd lea       rcx, [rip + 0x190729c]
0082b3c4 call      0x7ff7e9f465cf
0082b3c9 nop
0082b3ca cmp       qword ptr [rbp + 0x118], 0x10
0082b3d2 jb        0x7ff7ea73b3e0
0082b3d4 mov       rcx, qword ptr [rbp + 0x100]
0082b3db call      0x7ff7eade5628
0082b3e0 mov       qword ptr [rbp + 0x118], 0xf
0082b3eb mov       qword ptr [rbp + 0x110], rdi
0082b3f2 mov       byte ptr [rbp + 0x100], 0
0082b3f9 cmp       qword ptr [rbp + 0x28], 0x10
0082b3fe jb        0x7ff7ea73b409
0082b400 mov       rcx, qword ptr [rbp + 0x10]
0082b404 call      0x7ff7eade5628
0082b409 mov       qword ptr [rbp + 0x28], 0xf
0082b411 mov       qword ptr [rbp + 0x20], rdi
0082b415 mov       byte ptr [rbp + 0x10], 0
0082b419 lea       rcx, [rbp + 0x150]
0082b420 call      0x7ff7e9f2238c
0082b425 mov       rcx, qword ptr [rip + 0x1906f74]
0082b42c cmp       byte ptr [rbx + 0x21], 0
0082b430 jne       0x7ff7ea73b47e
0082b432 mov       rax, qword ptr [rbx + 0x10]
0082b436 cmp       byte ptr [rax + 0x21], 0
0082b43a jne       0x7ff7ea73b45e
0082b43c mov       rbx, rax
0082b43f mov       rax, qword ptr [rax]
0082b442 cmp       byte ptr [rax + 0x21], 0
0082b446 jne       0x7ff7ea73b47e
0082b448 nop       dword ptr [rax + rax]
0082b450 mov       rbx, rax
0082b453 mov       rax, qword ptr [rax]
0082b456 cmp       byte ptr [rax + 0x21], 0
0082b45a je        0x7ff7ea73b450
0082b45c jmp       0x7ff7ea73b47e
0082b45e mov       rax, qword ptr [rbx + 8]
0082b462 cmp       byte ptr [rax + 0x21], 0
0082b466 jne       0x7ff7ea73b47b
0082b468 cmp       rbx, qword ptr [rax + 0x10]
0082b46c jne       0x7ff7ea73b47b
0082b46e mov       rbx, rax
0082b471 mov       rax, qword ptr [rax + 8]
0082b475 cmp       byte ptr [rax + 0x21], 0
0082b479 je        0x7ff7ea73b468
0082b47b mov       rbx, rax
0082b47e cmp       rbx, rcx
0082b481 jne       0x7ff7ea73ae00
0082b487 lea       rcx, [rip + 0x1906e92]
0082b48e call      0x7ff7e9f4d186 ; ?clearMessages@InputHandler@@QEAAXXZ
0082b493 movss     xmm1, dword ptr [rip + 0x1907299]
0082b49b lea       rcx, [rip + 0x1906e0e]
0082b4a2 call      0x7ff7e9f4fbb1 ; ?mainloop@KingOfRenderThread@@QEAAXM@Z
0082b4a7 call      0x7ff7e9f47952
0082b4ac mov       rdx, qword ptr [rax]
0082b4af mov       rcx, rax
0082b4b2 call      qword ptr [rdx + 0x20]
0082b4b5 mov       rdx, qword ptr [rip + 0x1a1b7ac]
0082b4bc lea       rcx, [rip + 0x1906f69]
0082b4c3 call      qword ptr [rip + 0x1a1ad17]
0082b4c9 movzx     r11d, byte ptr [rip + 0x1906f42]
0082b4d1 mov       byte ptr [rip + 0x1906f3d], r11b
0082b4d8 movzx     eax, byte ptr [rip + 0x1906f35]
0082b4df mov       byte ptr [rip + 0x1906f31], al
0082b4e5 mov       rcx, rsi
0082b4e8 call      0x7ff7e9f47989
0082b4ed mov       al, 1
0082b4ef mov       rcx, qword ptr [rbp + 0x190]
0082b4f6 xor       rcx, rsp
0082b4f9 call      0x7ff7eade5db0
0082b4fe lea       r11, [rsp + 0x2a0]
0082b506 mov       rbx, qword ptr [r11 + 0x38]
0082b50a mov       rsi, qword ptr [r11 + 0x40]
0082b50e mov       rdi, qword ptr [r11 + 0x48]
0082b512 mov       rsp, r11
0082b515 pop       r15
0082b517 pop       r14
0082b519 pop       r13
0082b51b pop       r12
0082b51d pop       rbp
0082b51e ret
0082b51f int3
