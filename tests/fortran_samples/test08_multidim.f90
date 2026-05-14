! test08_multidim_array.f90
! Test 8: Multi-dimensional arrays and DOUBLE PRECISION in COMMON
! Tests that the collector correctly computes:
!   REAL*8 A(3,4) = 8 * 3 * 4 = 96 bytes
!   INTEGER B(2,5) = 4 * 2 * 5 = 40 bytes
! Total = 136 bytes

SUBROUTINE MULTIDIM_TEST
  IMPLICIT NONE
  REAL*8  :: A(3,4)      ! 8 * 3 * 4 = 96 bytes, offset 0
  INTEGER :: B(2,5)      ! 4 * 2 * 5 = 40 bytes, offset 96
  REAL    :: SCALAR      ! 4 bytes, offset 136
  COMMON /MATRICES/ A, B, SCALAR

  A(1,1) = 1.0D0
  B(1,1) = 42
  SCALAR = 3.14
END SUBROUTINE
