! test01_file_b.f90
! Test 1: Same /MYDATA/ block - GOOD case (both files agree on layout)
! File B declares same /MYDATA/ with 100 REALs + 1 INTEGER = 404 bytes

SUBROUTINE FILE_B_SUB
  IMPLICIT NONE
  REAL    :: X(100)
  INTEGER :: N
  COMMON /MYDATA/ X, N
  SAVE /MYDATA/

  WRITE(*,*) X(1), N
END SUBROUTINE
