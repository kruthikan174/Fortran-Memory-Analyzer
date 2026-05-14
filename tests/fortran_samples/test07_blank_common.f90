! test07_blank_common.f90
! Test 7: BLANK COMMON (unnamed COMMON block)
! Blank COMMON uses no name between //.
! Our tool should label it "__BLANK_COMMON__" in the JSON.

SUBROUTINE BLANK_COMMON_TEST
  IMPLICIT NONE
  REAL    :: ALPHA
  REAL    :: BETA
  INTEGER :: GAMMA

  COMMON ALPHA, BETA, GAMMA   ! ← blank COMMON, no /name/

  ALPHA = 1.0
  BETA  = 2.0
  GAMMA = 3
END SUBROUTINE
