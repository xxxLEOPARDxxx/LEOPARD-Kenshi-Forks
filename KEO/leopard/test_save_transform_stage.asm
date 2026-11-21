extern FixtureRead:qword
public FixturePair
public FixtureStockPair
public FixturePositionCall
public FixtureRotationCall
public FixtureHeaderPair, FixtureStockHeaderPair, FixtureTypeCall, FixtureIdCall
public FixtureReferencePair, FixtureStockReferencePair, FixtureRefStartCall, FixtureRefEndCall

.code
PairProcedure macro name, positionCall, rotationCall, firstSize, secondSize, offset, observeFirst
name proc frame
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    mov r8d, firstSize
positionCall::
    call qword ptr [FixtureRead]
    mov r8d, secondSize
    mov rdx, [rsp+28h]
    if observeFirst
        mov r10d, dword ptr [rdx]
        mov dword ptr [rdx+24], r10d
    endif
    lea rdx, [rdx+offset]
    mov rcx, [rsp+20h]
rotationCall::
    call qword ptr [FixtureRead]
    add rsp, 38h
    ret
name endp
endm

PairProcedure FixturePair, FixturePositionCall, FixtureRotationCall, 12, 16, 24, 0
PairProcedure FixtureStockPair, StockPositionCall, StockRotationCall, 12, 16, 24, 0
PairProcedure FixtureHeaderPair, FixtureTypeCall, FixtureIdCall, 4, 4, -16, 0
PairProcedure FixtureStockHeaderPair, StockTypeCall, StockIdCall, 4, 4, -16, 0
PairProcedure FixtureReferencePair, FixtureRefStartCall, FixtureRefEndCall, 4, 4, 16, 1
PairProcedure FixtureStockReferencePair, StockRefStartCall, StockRefEndCall, 4, 4, 16, 1
end
