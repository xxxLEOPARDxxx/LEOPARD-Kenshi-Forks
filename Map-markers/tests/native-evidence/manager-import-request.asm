?import@SaveManager@@QEAAXAEBUSaveInfo@@H@Z RVA=47aa80 slot=6197 PID=9388
0047aa80 mov       qword ptr [rsp + 8], rbx
0047aa85 mov       qword ptr [rsp + 0x10], rsi
0047aa8a push      rdi
0047aa8b sub       rsp, 0x40
0047aa8f cmp       dword ptr [rcx + 0xa0], 0
0047aa96 mov       esi, r8d
0047aa99 mov       rdi, rdx
0047aa9c mov       rbx, rcx
0047aa9f jne       0x7ff7ea38ab1d
0047aaa1 lea       rcx, [rip + 0x1cb7ca8]
0047aaa8 mov       dl, 1
0047aaaa call      0x7ff7e9f52f7d ; ?setLoading@ForgottenGUI@@QEAAX_N@Z
0047aaaf xor       r11d, r11d
0047aab2 lea       rcx, [rip + 0x125d627]
0047aab9 mov       dword ptr [rsp + 0x38], r11d
0047aabe mov       qword ptr [rsp + 0x30], r11
0047aac3 mov       dword ptr [rsp + 0x28], r11d
0047aac8 lea       edx, [r11 + 0x6e]
0047aacc xor       r9d, r9d
0047aacf xor       r8d, r8d
0047aad2 mov       qword ptr [rsp + 0x20], r11
0047aad7 call      0x7ff7ea9960c0
0047aadc lea       rdx, [rdi + 0x28]
0047aae0 lea       rcx, [rbx + 0xb0]
0047aae7 or        r9, 0xffffffffffffffff
0047aaeb xor       r8d, r8d
0047aaee mov       dword ptr [rbx + 0xa0], 3
0047aaf8 mov       dword ptr [rbx + 0xa4], esi
0047aafe call      0x7ff7e9f3808d
0047ab03 lea       rdx, [rdi + 0xb8]
0047ab0a lea       rcx, [rbx + 0xd8]
0047ab11 or        r9, 0xffffffffffffffff
0047ab15 xor       r8d, r8d
0047ab18 call      0x7ff7e9f3808d
0047ab1d mov       rbx, qword ptr [rsp + 0x50]
0047ab22 mov       rsi, qword ptr [rsp + 0x58]
0047ab27 add       rsp, 0x40
0047ab2b pop       rdi
0047ab2c ret
0047ab2d int3
