! test04_alignment_a.f90
! Test 4: ALIGNMENT VIOLATION
! A REAL*8 (DOUBLE PRECISION, 8 bytes) must start at an 8-byte aligned offset.
! Offset 4 is only 4-byte aligned → hardware fault or performance penalty on most CPUs.

SUBROUTINE ALIGN_A
  IMPLICIT NONE
  REAL    :: PAD       ! 4 bytes, offset 0  (OK)
  REAL*8  :: D         ! 8 bytes, offset 4  ← BAD: 4 % 8 != 0 → ALIGNMENT ERROR
  COMMON /ALIGNTEST/ PAD, D

  PAD = 1.0
  D   = 3.141592653589793D0
END SUBROUTINE
