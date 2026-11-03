0082ab7e call      0x7ff7e9f404d6 ; ?allThreadQueuesAreClear@GameWorld@@QEAA_NXZ
0082ab83 test      al, al
0082ab85 je        0x7ff7ea73ac53
0082ab8b mov       rcx, qword ptr [rip + 0x1908cae]
0082ab92 test      rcx, rcx
0082ab95 je        0x7ff7ea73ab9c
0082ab97 call      0x7ff7e9f3f58b ; ?waitForRunningStop_Blocking@ThreadClass@@QEAA_NXZ
0082ab9c call      0x7ff7e9f47952 ;
0082aba1 test      rax, rax
0082aba4 je        0x7ff7ea73abb3
0082aba6 call      0x7ff7e9f47952 ;
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
0082ac1f jmp       0x7ff7ea73b4ef ;
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
0082ac82 jmp       0x7ff7ea73b4ef ;
0082ac87 mov       rax, qword ptr [rip + 0x190762a]
0082ac8e mov       rcx, qword ptr [rax + 0x20]
0082ac92 test      rcx, rcx
0082ac95 je        0x7ff7ea73ac9c
0082ac97 call      0x7ff7e9f540b2 ;
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
0082ad53 call      0x7ff7e9f17676 ;
0082ad58 mov       rcx, rax
0082ad5b lea       rdx, [rsp + 0x48]
0082ad60 call      0x7ff7e9f205eb ;
0082ad65 lea       rdx, [rsp + 0x48]
0082ad6a mov       rcx, qword ptr [rip + 0x1908bef]
0082ad71 call      0x7ff7e9f4dfa5 ; ?shiftGroundEffects@ZoneManager@@QEAAXAEBVVector3@Ogre@@@Z
0082ad76 lea       rcx, [rsp + 0x48]
