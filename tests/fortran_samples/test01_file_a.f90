! test01_file_a.f90
! Test 1: Basic COMMON block - GOOD case (should produce no errors)
! File A declares /MYDATA/ with 100 REALs = 400 bytes

SUBROUTINE FILE_A_SUB
  IMPLICIT NONE
  REAL    :: X(100)
  INTEGER :: N
  COMMON /MYDATA/ X, N
  SAVE /MYDATA/

  X(1) = 3.14
  N    = 42
END SUBROUTINE
