! test09_character.f90
! Test 9: CHARACTER variables in COMMON
! CHARACTER*8 NAME = 8 bytes (1 byte per char × length 8)
! CHARACTER*4 CODE(10) = 4 * 10 = 40 bytes
! Mixing CHARACTER and non-CHARACTER in COMMON is actually non-standard
! but many compilers allow it — we should detect this.

SUBROUTINE CHAR_TEST
  IMPLICIT NONE
  CHARACTER*8  :: NAME       ! 8 bytes, offset 0
  CHARACTER*4  :: CODE(10)   ! 4 * 10 = 40 bytes, offset 8
  INTEGER      :: FLAG       ! 4 bytes, offset 48
  COMMON /STRINGS/ NAME, CODE, FLAG

  NAME    = 'FORTRAN '
  CODE(1) = 'OKAY'
  FLAG    = 1
END SUBROUTINE
