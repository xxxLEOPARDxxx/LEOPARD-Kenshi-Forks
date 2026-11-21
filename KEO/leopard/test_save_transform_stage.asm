extern FixtureRead:qword
public FixturePair
public FixtureStockPair
public FixturePositionCall
public FixtureRotationCall

.code
PairProcedure macro name, positionCall, rotationCall
name proc frame
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    mov r8d, 12
positionCall::
    call qword ptr [FixtureRead]
    mov r8d, 16
    mov rdx, [rsp+28h]
    lea rdx, [rdx+18h]
    mov rcx, [rsp+20h]
rotationCall::
    call qword ptr [FixtureRead]
    add rsp, 38h
    ret
name endp
endm

PairProcedure FixturePair, FixturePositionCall, FixtureRotationCall
PairProcedure FixtureStockPair, StockPositionCall, StockRotationCall
end
