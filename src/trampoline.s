	.section __TEXT,__text,regular,pure_instructions
	.global	_trampoline
_trampoline:
  ;; x0 result ptr
  ;; x1 target address
  stp     x29, x30, [sp, #-64]!
  stp     x19, x20, [sp, #16]
  stp     x21, x22, [sp, #32]
  mov     x29, sp

  ;; mov to non-volatile reg since we use it after bench
  mov     x19, x0
  ;; x20 counter
  mov     x20, 128
  ;; x21 start time
  mrs     x21, cntpct_el0

.loop:
  isb
  blr     x1
  isb
  subs     x20, x20, #1 ;; count--
  b.ne    .loop

  mrs     x0, cntpct_el0
  sub     x0, x0, x21 ;; x0 = end - start
  str     x0, [x19]
  
  ldp     x19, x20, [sp, #16]
  ldp     x21, x22, [sp, #32]
  ldp     x29, x30, [sp], #64
  ret
