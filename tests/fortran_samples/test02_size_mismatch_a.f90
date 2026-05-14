! test02_size_mismatch_a.f90
! Test 2: SIZE MISMATCH — File A declares 100 reals = 400 bytes

SUBROUTINE SIZE_A
  IMPLICIT NONE
  REAL :: X(100)      ! 100 * 4 = 400 bytes
  COMMON /BIGBLOCK/ X

  X(1) = 1.0
END SUBROUTINE
