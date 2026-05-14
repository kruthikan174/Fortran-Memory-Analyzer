! test02_size_mismatch_b.f90
! Test 2: SIZE MISMATCH — File B declares 200 integers = 800 bytes
! ⚠ BUG: /BIGBLOCK/ is 400 bytes in A, 800 bytes in B → OUT OF BOUNDS

SUBROUTINE SIZE_B
  IMPLICIT NONE
  INTEGER :: I(200)   ! 200 * 4 = 800 bytes  ← MISMATCH with test02_a
  COMMON /BIGBLOCK/ I

  I(1) = 99
END SUBROUTINE
