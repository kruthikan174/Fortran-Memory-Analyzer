! test06_save_inconsistent_a.f90
! Test 6: SAVE INCONSISTENCY
! File A has SAVE on /PERSIST/, File B does not.
! Standard says: if any declaration has SAVE, all should.
! Inconsistent SAVE → undefined behavior across compilation units.

SUBROUTINE SAVE_A
  IMPLICIT NONE
  INTEGER :: COUNTER
  COMMON /PERSIST/ COUNTER
  SAVE /PERSIST/           ! ← has SAVE

  COUNTER = COUNTER + 1
  WRITE(*,*) 'Counter:', COUNTER
END SUBROUTINE
