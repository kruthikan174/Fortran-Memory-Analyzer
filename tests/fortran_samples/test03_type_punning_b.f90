! test03_type_punning_b.f90
! Test 3: TYPE PUNNING — File B reads offset 0 as INTEGER
! ⚠ BUG: File A wrote REAL at offset 0, File B reads INTEGER at offset 0
!        The bit pattern is reinterpreted — silent data corruption

SUBROUTINE PUN_B
  IMPLICIT NONE
  INTEGER :: IRAW      ! 4 bytes, offset 0  ← READS A's REAL as INTEGER
  INTEGER :: IRAW2     ! 4 bytes, offset 4
  COMMON /PHYSICS/ IRAW, IRAW2

  WRITE(*,*) IRAW      ! garbage value — reads float bits as integer
END SUBROUTINE
