?_NV_mainLoop_GPUSensitiveStuff@GameWorld@@QEAAXM@Z RVA=787e70 slot=3649 PID=9388
00787e70 mov       rax, rsp
00787e73 push      rsi
00787e74 push      rdi
00787e75 push      r12
00787e77 sub       rsp, 0x70
00787e7b mov       qword ptr [rsp + 0x20], 0xfffffffffffffffe
00787e84 mov       qword ptr [rax + 0x10], rbx
00787e88 mov       qword ptr [rax + 0x18], rbp
00787e8c movaps    xmmword ptr [rax - 0x28], xmm6
00787e90 movaps    xmmword ptr [rax - 0x38], xmm7
00787e94 movaps    xmm6, xmm1
00787e97 mov       rsi, rcx
00787e9a mov       rcx, qword ptr [rcx + 0x790]
00787ea1 test      rcx, rcx
00787ea4 je        0x7ff7ea697ed8
00787ea6 call      0x7ff7e9f18f71 ; ?isRunning@ThreadClass@@QEAA_NXZ
00787eab test      al, al
00787ead je        0x7ff7ea697ed8
00787eaf mov       rbx, qword ptr [rsi + 0x790]
00787eb6 lea       rcx, [rsp + 0x90]
00787ebe call      0x7ff7e9f4d136
00787ec3 mov       rdx, rax
00787ec6 lea       rcx, [rbx + 0x68]
00787eca call      0x7ff7e9f5f8ae
00787ecf lea       rcx, [rbx + 0x68]
00787ed3 call      0x7ff7e9f3e843
00787ed8 mov       rcx, rsi
00787edb call      0x7ff7e9f41372 ; ?processThreadMessages@GameWorld@@QEAAXXZ
00787ee0 movaps    xmm5, xmm6
00787ee3 mulss     xmm5, dword ptr [rsi + 0x700]
00787eeb movss     dword ptr [rip + 0x19aa845], xmm5
00787ef3 movss     dword ptr [rip + 0x19aa839], xmm5
00787efb xorps     xmm7, xmm7
00787efe comiss    xmm7, xmm5
00787f01 jb        0x7ff7ea697f13
00787f03 movss     xmm0, dword ptr [rip + 0xef9bcd]
00787f0b movss     dword ptr [rip + 0x19aa821], xmm0
00787f13 movss     dword ptr [rip + 0x19aa815], xmm6
00787f1b cmp       byte ptr [rsi + 0x8b9], 0
00787f22 je        0x7ff7ea697f2c
00787f24 movss     dword ptr [rip + 0x19aa80c], xmm7
00787f2c mov       rax, qword ptr [rip + 0x19aa385]
00787f33 mov       dl, 1
00787f35 mov       rcx, qword ptr [rax + 0x58]
00787f39 call      qword ptr [rip + 0x1ac00e1]
00787f3f lea       rcx, [rip + 0x19acbca]
00787f46 mov       rdx, rax
00787f49 mov       r8d, 0x40
00787f4f call      0x7ff7eade5c18
00787f54 mov       rcx, rsi
00787f57 call      0x7ff7e9f26aa4 ; ?processKeys@GameWorld@@QEAAXXZ
00787f5c mov       r11d, dword ptr [rip + 0x19a5c71]
00787f63 test      r11b, 1
00787f67 jne       0x7ff7ea697f9e
00787f69 or        r11d, 1
00787f6d mov       dword ptr [rip + 0x19a5c60], r11d
00787f74 mov       ecx, 0x120
00787f79 call      qword ptr [rip + 0x1abe201]
00787f7f mov       qword ptr [rsp + 0x90], rax
00787f87 test      rax, rax
00787f8a je        0x7ff7ea697f95
00787f8c mov       rcx, rax
00787f8f call      0x7ff7e9f444aa ; ??0SaveManager@@QEAA@XZ | ?_CONSTRUCTOR@SaveManager@@QEAAPEAV1@XZ
00787f94 nop
00787f95 mov       qword ptr [rip + 0x19a5c2c], rax
00787f9c jmp       0x7ff7ea697fa5
00787f9e mov       rax, qword ptr [rip + 0x19a5c23]
00787fa5 mov       rcx, rax
00787fa8 call      0x7ff7e9f50872 ; ?updateAutoSave@SaveManager@@QEAAXXZ
00787fad mov       r11d, dword ptr [rip + 0x199ec48]
00787fb4 test      r11b, 1
00787fb8 jne       0x7ff7ea697fef
00787fba or        r11d, 1
00787fbe mov       dword ptr [rip + 0x199ec37], r11d
00787fc5 mov       ecx, 0x2f0
00787fca call      qword ptr [rip + 0x1abe1f8]
00787fd0 mov       qword ptr [rsp + 0x90], rax
00787fd8 test      rax, rax
00787fdb je        0x7ff7ea697fe6
00787fdd mov       rcx, rax
00787fe0 call      0x7ff7e9f420ab ; ??0ResourceLoader@@QEAA@XZ | ?_CONSTRUCTOR@ResourceLoader@@QEAAPEAV1@XZ
00787fe5 nop
00787fe6 mov       qword ptr [rip + 0x199ec03], rax
00787fed jmp       0x7ff7ea697ff6
00787fef mov       rax, qword ptr [rip + 0x199ebfa]
00787ff6 mov       rcx, rax
00787ff9 call      0x7ff7e9f1fd99 ; ?updateMT@ResourceLoader@@QEAAXXZ
00787ffe lea       rcx, [rip + 0x19ab0ab]
00788005 call      0x7ff7e9f258cf ; ?processUpdateRemovalList@GameWorld@@QEAAXXZ
0078800a mov       rcx, qword ptr [rsi + 0x18]
0078800e call      0x7ff7e9f18f71 ; ?isRunning@ThreadClass@@QEAA_NXZ
00788013 test      al, al
00788015 jne       0x7ff7ea698170
0078801b xor       edx, edx
0078801d mov       rcx, rsi
00788020 call      0x7ff7e9f589f5 ; ?processKillList@GameWorld@@QEAAX_N@Z
00788025 mov       rcx, qword ptr [rsi + 0x18]
00788029 mov       rax, qword ptr [rcx]
0078802c call      qword ptr [rax + 0x20]
0078802f cmp       byte ptr [rip + 0x19ab933], 0
00788036 jne       0x7ff7ea698040
00788038 mov       rcx, rsi
0078803b call      0x7ff7e9f38d5d ; ?charsUpdateUT@GameWorld@@QEAAXXZ
00788040 mov       rcx, qword ptr [rsi + 0x580]
00788047 call      0x7ff7e9f50b47 ; ?updateUT@PlayerInterface@@QEAAXXZ
0078804c mov       r11d, dword ptr [rip + 0x19a5bc1]
00788053 test      r11b, 1
00788057 jne       0x7ff7ea69808d
00788059 or        r11d, 1
0078805d mov       dword ptr [rip + 0x19a5bb0], r11d
00788064 mov       ecx, 0x318
00788069 call      0x7ff7eade563a
0078806e mov       qword ptr [rsp + 0x90], rax
00788076 test      rax, rax
00788079 je        0x7ff7ea698084
0078807b mov       rcx, rax
0078807e call      0x7ff7e9f300d6 ; ??0SaveFileSystem@@QEAA@XZ | ?_CONSTRUCTOR@SaveFileSystem@@QEAAPEAV1@XZ
00788083 nop
00788084 mov       qword ptr [rip + 0x19a5b7d], rax
0078808b jmp       0x7ff7ea698094
0078808d mov       rax, qword ptr [rip + 0x19a5b74]
00788094 mov       rcx, rax
00788097 call      0x7ff7e9f39325 ; ?sync@SaveFileSystem@@QEAAXXZ
0078809c mov       rcx, qword ptr [rsi + 0x18]
007880a0 call      0x7ff7e9f3505e ; ?queuesAreClearMT@PhysicsInterface@@QEAA_NXZ
007880a5 test      al, al
007880a7 je        0x7ff7ea6980b6
007880a9 call      0x7ff7e9f42c4f ; ?getSingleton@SaveManager@@SAPEAV1@XZ
007880ae mov       rcx, rax
007880b1 call      0x7ff7e9f27607 ; ?execute@SaveManager@@QEAAXXZ
007880b6 mov       rcx, rsi
007880b9 call      0x7ff7e9f64089 ; ?threadSafeRagdollUpdates@GameWorld@@QEAAXXZ
007880be mov       r11d, dword ptr [rip + 0x19a9d57]
007880c5 test      r11b, 1
007880c9 jne       0x7ff7ea6980ff
007880cb or        r11d, 1
007880cf mov       dword ptr [rip + 0x19a9d46], r11d
007880d6 mov       ecx, 0x18
007880db call      0x7ff7eade563a
007880e0 mov       qword ptr [rsp + 0x90], rax
007880e8 test      rax, rax
007880eb je        0x7ff7ea6980f6
007880ed mov       rcx, rax
007880f0 call      0x7ff7e9f5f106
007880f5 nop
007880f6 mov       qword ptr [rip + 0x19a9d13], rax
007880fd jmp       0x7ff7ea698106
007880ff mov       rax, qword ptr [rip + 0x19a9d0a]
00788106 mov       rcx, rax
00788109 call      0x7ff7e9f16b22
0078810e mov       r11d, dword ptr [rip + 0x199f077]
00788115 test      r11b, 1
00788119 jne       0x7ff7ea698150
0078811b or        r11d, 1
0078811f mov       dword ptr [rip + 0x199f066], r11d
00788126 mov       ecx, 0x288
0078812b call      qword ptr [rip + 0x1abe04f]
00788131 mov       qword ptr [rsp + 0x90], rax
00788139 test      rax, rax
0078813c je        0x7ff7ea698147
0078813e mov       rcx, rax
00788141 call      0x7ff7e9f4d532 ; ??0WeatherSystem@@QEAA@XZ | ?_CONSTRUCTOR@WeatherSystem@@QEAAPEAV1@XZ
00788146 nop
00788147 mov       qword ptr [rip + 0x199f032], rax
0078814e jmp       0x7ff7ea698157
00788150 mov       rax, qword ptr [rip + 0x199f029]
00788157 mov       rcx, rax
0078815a call      0x7ff7e9f57258 ; ?updateUT@WeatherSystem@@QEAAXXZ
0078815f movss     xmm1, dword ptr [rip + 0x19aa5c9]
00788167 mov       rcx, qword ptr [rsi + 0x18]
0078816b call      0x7ff7e9f59071 ; ?startRunning@ThreadClass@@QEAA_NM@Z
00788170 mov       rcx, qword ptr [rsi + 0x8c0]
00788177 call      0x7ff7e9f18f71 ; ?isRunning@ThreadClass@@QEAA_NXZ
0078817c test      al, al
0078817e jne       0x7ff7ea698194
00788180 movss     xmm1, dword ptr [rip + 0x19aa5a8]
00788188 mov       rcx, qword ptr [rsi + 0x8c0]
0078818f call      0x7ff7e9f59071 ; ?startRunning@ThreadClass@@QEAA_NM@Z
00788194 movss     xmm1, dword ptr [rip + 0x19aa594]
0078819c mov       rcx, qword ptr [rsi + 0x4e0]
007881a3 call      0x7ff7e9f1207c
007881a8 mov       r11, qword ptr [rip + 0x19ab481]
007881af mov       rdi, qword ptr [r11 + 0x30]
007881b3 mov       rcx, qword ptr [rdi + 0x68]
007881b7 call      qword ptr [rip + 0x1abefbb]
007881bd mov       rcx, rax
007881c0 lea       rdx, [rsp + 0x34]
007881c5 call      qword ptr [rip + 0x1abef2d]
007881cb mov       rbx, rax
007881ce lea       rdx, [rsp + 0x40]
007881d3 mov       rcx, qword ptr [rdi + 0x70]
007881d7 call      qword ptr [rip + 0x1abef8b]
007881dd mov       rcx, rax
007881e0 mov       r8, rbx
007881e3 lea       rdx, [rsp + 0x28]
007881e8 call      qword ptr [rip + 0x1abee2a]
007881ee lea       rdx, [rsp + 0x28]
007881f3 mov       rcx, qword ptr [rsi + 0x8b0]
007881fa call      0x7ff7e9f56fc9 ; ?updateGPUSafeThread@ZoneManager@@QEAAXAEBVVector3@Ogre@@@Z
007881ff call      0x7ff7e9f51eb6 ; ?updateEffects@CombatClass@@SAXXZ
00788204 mov       rcx, qword ptr [rsi + 0x580]
0078820b call      0x7ff7e9f484fb ; ?update@PlayerInterface@@QEAAXXZ
00788210 mov       r11d, dword ptr [rip + 0x19a59ed]
00788217 test      r11b, 1
0078821b jne       0x7ff7ea698252
0078821d or        r11d, 1
00788221 mov       dword ptr [rip + 0x19a59dc], r11d
00788228 mov       ecx, 0x88
0078822d call      qword ptr [rip + 0x1abdf4d]
00788233 mov       qword ptr [rsp + 0x90], rax
0078823b test      rax, rax
0078823e je        0x7ff7ea698249
00788240 mov       rcx, rax
00788243 call      0x7ff7e9f64232
00788248 nop
00788249 mov       qword ptr [rip + 0x19a59a8], rax
00788250 jmp       0x7ff7ea698259
00788252 mov       rax, qword ptr [rip + 0x19a599f]
00788259 mov       rcx, rax
0078825c call      0x7ff7e9f28fb1
00788261 mov       r11d, dword ptr [rip + 0x199ef34]
00788268 test      r11b, 1
0078826c jne       0x7ff7ea6982a3
