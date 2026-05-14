! test03_type_punning_a.f90
! Test 3: TYPE PUNNING — File A writes a REAL at offset 0

SUBROUTINE PUN_A
  IMPLICIT NONE
  REAL :: VOLTAGE      ! 4 bytes, offset 0
  REAL :: CURRENT      ! 4 bytes, offset 4
  COMMON /PHYSICS/ VOLTAGE, CURRENT

  VOLTAGE = 3.3
  CURRENT = 0.5
END SUBROUTINE
