jmp math

cube:
    dup
    over
    mul
    mul
    ret

square:
    dup
    mul
    ret

fact:
    dup
    1;
    lte;
    jz factloop
    drop
    1;
    ret

factloop:
    dup
    1;
    sub
    call fact
    mul
    ret

abs:
    dup
    0;
    lt;
    jz absend
    0;
    swap
    sub

absend:
    ret

min:
    over
    over
    lt;
    jz minsec
    drop
    ret

minsec:
    swap
    drop
    ret

max:
    over
    over
    gt;
    jz maxsec
    drop
    ret

maxsec:
    swap
    drop
    ret

mod:
    over
    over
    div
    mul
    sub
    ret

math:
