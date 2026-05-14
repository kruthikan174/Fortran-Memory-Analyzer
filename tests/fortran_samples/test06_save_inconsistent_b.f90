! test06_save_inconsistent_b.f90
! Test 6: SAVE INCONSISTENCY — File B has NO SAVE on /PERSIST/
! ⚠ BUG: File A saves the block, File B does not → undefined behavior

SUBROUTINE SAVE_B
  IMPLICIT NONE
  INTEGER :: COUNTER
  COMMON /PERSIST/ COUNTER
  ! No SAVE here ← INCONSISTENT with test06_a

  WRITE(*,*) 'Counter from B:', COUNTER  ! may read stale/garbage value
END SUBROUTINE
