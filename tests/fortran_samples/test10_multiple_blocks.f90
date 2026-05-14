! test10_multiple_blocks.f90
! Test 10: One subroutine participates in MULTIPLE COMMON blocks
! The collector must extract both /BLOCKA/ and /BLOCKB/ separately.

SUBROUTINE MULTI_BLOCK_TEST
  IMPLICIT NONE

  ! Block A: physics constants
  REAL*8 :: PI
  REAL*8 :: E_CONST
  COMMON /PHYSICS_CONST/ PI, E_CONST   ! 8 + 8 = 16 bytes

  ! Block B: simulation state
  INTEGER :: STEP
  REAL    :: TIME
  REAL    :: DT
  COMMON /SIM_STATE/ STEP, TIME, DT    ! 4 + 4 + 4 = 12 bytes

  SAVE /PHYSICS_CONST/
  SAVE /SIM_STATE/

  PI      = 3.141592653589793D0
  E_CONST = 2.718281828459045D0
  STEP    = 0
  TIME    = 0.0
  DT      = 0.001
END SUBROUTINE
