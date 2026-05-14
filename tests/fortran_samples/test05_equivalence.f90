! test05_equivalence.f90
! Test 5: EQUIVALENCE — local array aliasing a COMMON variable (valid case)
! EQUIVALENCE (X, XALIAS) means X and XALIAS refer to the same storage.
! Both are scalars; XALIAS is local, X is in COMMON.
! Our tool records this pair so Person 2 can flag aliasing of COMMON vars.

SUBROUTINE EQUIV_TEST
  IMPLICIT NONE
  REAL :: X            ! In COMMON /EQTEST/ — 4 bytes at offset 0
  REAL :: Y            ! In COMMON /EQTEST/ — 4 bytes at offset 4
  REAL :: XALIAS       ! Local variable
  COMMON /EQTEST/ X, Y
  EQUIVALENCE (X, XALIAS)   ! XALIAS shares storage with X

  X = 99.0
  WRITE(*,*) XALIAS    ! prints 99.0
END SUBROUTINE
