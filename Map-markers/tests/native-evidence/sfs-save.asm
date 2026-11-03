?saveGame@SaveFileSystem@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z RVA=473380 slot=6169 PID=9388
00473380 push      rbp
00473382 push      rsi
00473383 push      rdi
00473384 push      r12
00473386 push      r13
00473388 push      r14
0047338a push      r15
0047338c lea       rbp, [rsp - 0xd0]
00473394 sub       rsp, 0x1d0
0047339b mov       qword ptr [rbp - 0x78], 0xfffffffffffffffe
004733a3 mov       qword ptr [rsp + 0x220], rbx
004733ab mov       rax, qword ptr [rip + 0x1ca3006]
004733b2 xor       rax, rsp
004733b5 mov       qword ptr [rbp + 0xc0], rax
004733bc mov       r14, rdx
004733bf mov       rsi, rcx
004733c2 xor       r13d, r13d
004733c5 mov       dword ptr [rsp + 0x20], r13d
004733ca mov       dword ptr [rcx + 0x1e0], 1
004733d4 mov       rcx, qword ptr [rcx + 0x1c0]
004733db mov       rax, qword ptr [rcx]
004733de mov       dl, 1
004733e0 call      qword ptr [rax + 0x18]
004733e3 mov       r9, qword ptr [rsi + 0x158]
004733ea lea       r12, [rsi + 0x148]
004733f1 dec       r9
004733f4 xor       r8d, r8d
004733f7 lea       rdx, [rbp + 8]
004733fb mov       rcx, r12
004733fe call      0x7ff7e9f55a07
00473403 nop
00473404 mov       qword ptr [rbp - 8], 0xf
0047340c mov       qword ptr [rbp - 0x10], r13
00473410 mov       byte ptr [rbp - 0x20], r13b
00473414 or        r9, 0xffffffffffffffff
00473418 xor       r8d, r8d
0047341b lea       rdx, [rsi + 0x120]
00473422 lea       rcx, [rbp - 0x20]
00473426 call      0x7ff7e9f3808d
0047342b nop
0047342c mov       qword ptr [rbp - 0x58], 0xf
00473434 mov       qword ptr [rbp - 0x60], r13
00473438 mov       byte ptr [rbp - 0x70], r13b
0047343c or        r9, 0xffffffffffffffff
00473440 xor       r8d, r8d
00473443 mov       rdx, r14
00473446 lea       rcx, [rbp - 0x70]
0047344a call      0x7ff7e9f3808d
0047344f nop
00473450 mov       r8, r14
00473453 lea       rdx, [rip + 0x12076be]
0047345a lea       rcx, [rbp - 0x48]
0047345e call      0x7ff7e9f3851f
00473463 nop
00473464 lea       edx, [r13 + 2]
00473468 mov       rcx, rax
0047346b call      0x7ff7e9f43d9d ; ?logMessage@Logger@@SAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@1@@Z
00473470 nop
00473471 cmp       qword ptr [rbp - 0x30], 0x10
00473476 jb        0x7ff7ea383481
00473478 mov       rcx, qword ptr [rbp - 0x48]
0047347c call      0x7ff7eade5628
00473481 mov       r8, r14
00473484 lea       rdx, [rip + 0x1263dfd]
0047348b lea       rcx, [rbp + 0x30]
0047348f call      0x7ff7e9f3851f
00473494 nop
00473495 lea       r8, [rip + 0x1263dcc]
0047349c mov       rdx, rax
0047349f lea       rcx, [rbp - 0x48]
004734a3 call      0x7ff7e9f42d6c
004734a8 nop
004734a9 mov       r8d, 2
004734af mov       rdx, rax
004734b2 mov       rcx, rsi
004734b5 call      0x7ff7e9f1a68c ; ?log@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@W4Severity@Logger@@@Z
004734ba nop
004734bb cmp       qword ptr [rbp - 0x30], 0x10
004734c0 jb        0x7ff7ea3834cb
004734c2 mov       rcx, qword ptr [rbp - 0x48]
004734c6 call      0x7ff7eade5628
004734cb mov       qword ptr [rbp - 0x30], 0xf
004734d3 mov       qword ptr [rbp - 0x38], r13
004734d7 mov       byte ptr [rbp - 0x48], 0
004734db cmp       qword ptr [rbp + 0x48], 0x10
004734e0 jb        0x7ff7ea3834eb
004734e2 mov       rcx, qword ptr [rbp + 0x30]
004734e6 call      0x7ff7eade5628
004734eb mov       qword ptr [rbp + 0x80], 0xf
004734f6 mov       qword ptr [rbp + 0x78], r13
004734fa mov       byte ptr [rbp + 0x68], 0
004734fe mov       qword ptr [rbp + 0xa8], 0xf
00473509 mov       qword ptr [rbp + 0xa0], r13
00473510 mov       byte ptr [rbp + 0x90], 0
00473517 lea       rdx, [rsi + 0x120]
0047351e mov       rcx, r14
00473521 call      0x7ff7e9f34037
00473526 test      eax, eax
00473528 jne       0x7ff7ea383595
0047352a mov       qword ptr [rsp + 0x48], 0xf
00473533 mov       qword ptr [rsp + 0x40], r13
00473538 mov       byte ptr [rsp + 0x30], al
0047353c mov       qword ptr [rsp + 0x70], 0xf
00473545 mov       qword ptr [rsp + 0x68], r13
0047354a mov       byte ptr [rsp + 0x58], al
0047354e lea       rdx, [rsp + 0x28]
00473553 lea       rcx, [rsi + 0x1c8]
0047355a call      0x7ff7e9f191d3
0047355f mov       edi, dword ptr [rsi + 0x1d0]
00473565 dec       edi
00473567 imul      rdi, rdi, 0x58
0047356b add       rdi, qword ptr [rsi + 0x1d8]
00473572 mov       dword ptr [rdi], 1
00473578 lea       rcx, [rdi + 8]
0047357c or        r9, 0xffffffffffffffff
00473580 xor       r8d, r8d
00473583 lea       rdx, [rbp + 8]
00473587 call      0x7ff7e9f3808d
0047358c lea       rcx, [rdi + 0x30]
00473590 jmp       0x7ff7ea383775
00473595 mov       qword ptr [rbp - 0x30], 7
0047359d mov       qword ptr [rbp - 0x38], r13
004735a1 mov       word ptr [rbp - 0x48], r13w
004735a6 mov       r13, qword ptr [rbp - 0x60]
004735aa test      r13, r13
004735ad je        0x7ff7ea3835e4
004735af lea       rdi, [rbp - 0x70]
004735b3 mov       rcx, qword ptr [rbp - 0x70]
004735b7 mov       rax, qword ptr [rbp - 0x58]
004735bb cmp       rax, 0x10
004735bf cmovae    rdi, rcx
004735c3 lea       rbx, [rbp - 0x70]
004735c7 cmovae    rbx, rcx
004735cb call      0x7ff7eadefc90
004735d0 mov       r9, rax
004735d3 lea       rdx, [rdi + r13]
004735d7 lea       r8, [rbp - 0x48]
004735db mov       rcx, rbx
004735de call      0x7ff7eadef440
004735e3 nop
004735e4 mov       dword ptr [rsp + 0x20], 1
004735ec xor       r8d, r8d
004735ef lea       rdx, [rbp - 0x48]
004735f3 lea       rcx, [rbp - 0x80]
004735f7 call      0x7ff7eadf79d0
004735fc mov       ecx, dword ptr [rax]
004735fe test      ecx, ecx
00473600 je        0x7ff7ea383618
00473602 cmp       ecx, 1
00473605 je        0x7ff7ea383618
00473607 lea       rcx, [rbp - 0x70]
0047360b call      0x7ff7e9f632ba
00473610 test      al, al
00473612 jne       0x7ff7ea383618
00473614 mov       bl, 1
00473616 jmp       0x7ff7ea38361a
00473618 xor       bl, bl
0047361a cmp       qword ptr [rbp - 0x30], 8
0047361f jb        0x7ff7ea38362a
00473621 mov       rcx, qword ptr [rbp - 0x48]
00473625 call      0x7ff7eade5628
0047362a test      bl, bl
0047362c je        0x7ff7ea38368d
0047362e xor       ebx, ebx
00473630 mov       dword ptr [rsi + 0x1e0], ebx
00473636 cmp       qword ptr [rbp - 0x58], 0x10
0047363b jb        0x7ff7ea383646
0047363d mov       rcx, qword ptr [rbp - 0x70]
00473641 call      0x7ff7eade5628
00473646 mov       qword ptr [rbp - 0x58], 0xf
0047364e mov       qword ptr [rbp - 0x60], rbx
00473652 mov       byte ptr [rbp - 0x70], 0
00473656 cmp       qword ptr [rbp - 8], 0x10
0047365b jb        0x7ff7ea383666
0047365d mov       rcx, qword ptr [rbp - 0x20]
00473661 call      0x7ff7eade5628
00473666 mov       qword ptr [rbp - 8], 0xf
0047366e mov       qword ptr [rbp - 0x10], rbx
00473672 mov       byte ptr [rbp - 0x20], 0
00473676 cmp       qword ptr [rbp + 0x20], 0x10
0047367b jb        0x7ff7ea383686
0047367d mov       rcx, qword ptr [rbp + 8]
00473681 call      0x7ff7eade5628
00473686 xor       al, al
00473688 jmp       0x7ff7ea383853
0047368d mov       qword ptr [rsp + 0x48], 0xf
00473696 xor       r13d, r13d
00473699 mov       qword ptr [rsp + 0x40], r13
0047369e mov       byte ptr [rsp + 0x30], r13b
004736a3 mov       qword ptr [rsp + 0x70], 0xf
004736ac mov       qword ptr [rsp + 0x68], r13
004736b1 mov       byte ptr [rsp + 0x58], r13b
004736b6 lea       rdx, [rsp + 0x28]
004736bb lea       rcx, [rsi + 0x1c8]
004736c2 call      0x7ff7e9f191d3
004736c7 mov       eax, dword ptr [rsi + 0x1d0]
004736cd dec       eax
004736cf mov       ebx, eax
004736d1 imul      rbx, rbx, 0x58
004736d5 add       rbx, qword ptr [rsi + 0x1d8]
004736dc mov       dword ptr [rbx], r13d
004736df lea       rcx, [rbx + 8]
004736e3 or        r9, 0xffffffffffffffff
004736e7 xor       r8d, r8d
004736ea lea       rdx, [rbp + 8]
004736ee call      0x7ff7e9f3808d
004736f3 lea       rcx, [rbx + 0x30]
004736f7 or        r9, 0xffffffffffffffff
004736fb xor       r8d, r8d
004736fe lea       rdx, [rbp - 0x70]
00473702 call      0x7ff7e9f3808d
00473707 cmp       qword ptr [rsi + 0x130], r13
0047370e je        0x7ff7ea383785
00473710 mov       qword ptr [rsp + 0x48], 0xf
00473719 mov       qword ptr [rsp + 0x40], r13
0047371e mov       byte ptr [rsp + 0x30], r13b
00473723 mov       qword ptr [rsp + 0x70], 0xf
0047372c mov       qword ptr [rsp + 0x68], r13
00473731 mov       byte ptr [rsp + 0x58], r13b
00473736 lea       rdx, [rsp + 0x28]
0047373b lea       rcx, [rsi + 0x1c8]
00473742 call      0x7ff7e9f191d3
00473747 mov       ebx, dword ptr [rsi + 0x1d0]
0047374d dec       ebx
0047374f imul      rbx, rbx, 0x58
00473753 add       rbx, qword ptr [rsi + 0x1d8]
0047375a mov       dword ptr [rbx], r13d
0047375d lea       rcx, [rbx + 8]
00473761 or        r9, 0xffffffffffffffff
00473765 xor       r8d, r8d
00473768 lea       rdx, [rbp - 0x20]
0047376c call      0x7ff7e9f3808d
00473771 lea       rcx, [rbx + 0x30]
00473775 or        r9, 0xffffffffffffffff
00473779 xor       r8d, r8d
0047377c lea       rdx, [rbp - 0x70]
00473780 call      0x7ff7e9f3808d
00473785 or        r9d, 0xffffffff
00473789 xor       r8d, r8d
0047378c xorps     xmm1, xmm1
0047378f mov       rcx, rsi
00473792 call      0x7ff7e9f1c379 ; ?beginThread@ThreadClass@@QEAA_NM_NH@Z
00473797 xorps     xmm1, xmm1
0047379a mov       rcx, rsi
0047379d call      0x7ff7e9f59071 ; ?startRunning@ThreadClass@@QEAA_NM@Z
004737a2 or        r9, 0xffffffffffffffff
004737a6 xor       r8d, r8d
004737a9 mov       rdx, r14
004737ac lea       rcx, [rsi + 0x120]
004737b3 call      0x7ff7e9f3808d
004737b8 mov       qword ptr [rbp - 0x30], 0xf
004737c0 mov       qword ptr [rbp - 0x38], r13
004737c4 mov       byte ptr [rbp - 0x48], 0
004737c8 or        r9, 0xffffffffffffffff
004737cc xor       r8d, r8d
004737cf mov       rdx, r12
004737d2 lea       rcx, [rbp - 0x48]
004737d6 call      0x7ff7e9f3808d
004737db lea       rcx, [rsi + 0x108]
004737e2 lea       rdx, [rbp - 0x48]
004737e6 call      0x7ff7e9f644b7
004737eb mov       qword ptr [r12 + 0x10], r13
004737f0 cmp       qword ptr [r12 + 0x18], 0x10
004737f6 jb        0x7ff7ea3837fc
004737f8 mov       r12, qword ptr [r12]
004737fc mov       byte ptr [r12], 0
00473801 cmp       qword ptr [rbp - 0x58], 0x10
00473806 jb        0x7ff7ea383811
00473808 mov       rcx, qword ptr [rbp - 0x70]
0047380c call      0x7ff7eade5628
00473811 mov       qword ptr [rbp - 0x58], 0xf
00473819 mov       qword ptr [rbp - 0x60], r13
0047381d mov       byte ptr [rbp - 0x70], 0
00473821 cmp       qword ptr [rbp - 8], 0x10
00473826 jb        0x7ff7ea383831
00473828 mov       rcx, qword ptr [rbp - 0x20]
0047382c call      0x7ff7eade5628
00473831 mov       qword ptr [rbp - 8], 0xf
00473839 mov       qword ptr [rbp - 0x10], r13
0047383d mov       byte ptr [rbp - 0x20], 0
00473841 cmp       qword ptr [rbp + 0x20], 0x10
00473846 jb        0x7ff7ea383851
00473848 mov       rcx, qword ptr [rbp + 8]
0047384c call      0x7ff7eade5628
00473851 mov       al, 1
00473853 mov       rcx, qword ptr [rbp + 0xc0]
0047385a xor       rcx, rsp
0047385d call      0x7ff7eade5db0
00473862 mov       rbx, qword ptr [rsp + 0x220]
0047386a add       rsp, 0x1d0
00473871 pop       r15
00473873 pop       r14
00473875 pop       r13
00473877 pop       r12
00473879 pop       rdi
0047387a pop       rsi
0047387b pop       rbp
0047387c ret
0047387d int3
