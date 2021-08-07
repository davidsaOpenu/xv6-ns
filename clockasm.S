# Time measurement since boot
.extern cycles_per_jiffy
.globl ticks_now
ticks_now:
    rdtsc
    mov cycles_per_jiffy, %ecx
    div %ecx
    ret

.extern cycles_per_millisecond
.globl steady_clock_now
steady_clock_now:
    rdtsc
    mov cycles_per_millisecond, %ecx
    div %ecx
    ret

# Time measurment from a timestamp
.globl timestamp_now
timestamp_now:
    rdtsc
    mov 4(%esp), %ecx # uint[2]
    mov %eax, (%ecx)
    mov %edx, 4(%ecx)
    ret

.extern cycles_per_microsecond
.globl us_since_ts
us_since_ts:
    rdtsc
    mov 4(%esp), %ecx # uint[2] - prev timestamp
    sub (%ecx), %eax
    sbb 4(%ecx), %edx
    mov cycles_per_microsecond, %ecx
    div %ecx
    ret

# TSC / APIC frequency measurement
.globl tscfreq
tscfreq:
    call pitinit
    rdtsc
    mov %eax, %ecx
    mov $0x2E, %al
    call pitwait
    rdtsc
    sub %ecx, %eax
    ret

.globl picfreq
picfreq:
    call pitinit
    mov 4(%esp), %ecx
    mov (%ecx), %edx # lapic[TCCR] before 10ms interval
    call pitwait
    mov %edx, %eax
    mov (%ecx), %edx # lapic[TCCR] after interval
    sub %edx, %eax
    ret

# PIT operations
pitinit:
    xor %ax, %ax
warmup:
    dec %ax
    jnz warmup
    mov $0x34, %al # operation mode
    out %al, $0x43
    mov $0x9C, %al # *
    out %al, $0x40 #  } interval, 0x2E9C = 10 milliseconds
    mov $0x2E, %al # *
    out %al, $0x40
    ret

pitwait:
    mov %al, %ah
    xor %al, %al
    out %al, $0x43 # latch timer value for reading
    in $0x40, %al
    in $0x40, %al # high order byte
    cmp %al, %ah # wait for timer restart
    jge pitwait
    ret