10 REM Function Call Performance Benchmark
20 REM Tests function call overhead and math functions
30 REM ===============================================
40 CLS
50 PRINT "FUNCTION CALL PERFORMANCE BENCHMARK"
60 PRINT "==================================="
70 PRINT ""
80 REM
90 REM Test 1: User-Defined Function Calls
100 REM
110 PRINT "Test 1: User-defined functions (5000 calls)"
120 DEF FNDouble(X) = X * 2
130 DEF FNSquare(X) = X * X
140 SUM = 0
150 FOR I% = 1 TO 5000
160 SUM = SUM + FNDouble(I%)
170 NEXT I%
180 PRINT "  Double sum: "; SUM
190 SUM2 = 0
200 FOR I% = 1 TO 5000
210 SUM2 = SUM2 + FNSquare(I%)
220 NEXT I%
230 PRINT "  Square sum: "; SUM2
240 PRINT ""
250 REM
260 REM Test 2: Built-in Math Functions
270 REM
280 PRINT "Test 2: Built-in math functions (1000 calls)"
290 ABSSUM = 0
300 FOR I% = 1 TO 1000
310 ABSSUM = ABSSUM + ABS(-I%)
320 NEXT I%
330 PRINT "  ABS sum: "; ABSSUM
340 INTSUM = 0
350 FOR I% = 1 TO 1000
360 INTSUM = INTSUM + INT(I% * 1.5)
370 NEXT I%
380 PRINT "  INT sum: "; INTSUM
390 PRINT ""
400 REM
410 REM Test 3: Trigonometric Functions
420 REM
430 PRINT "Test 3: Trig functions (1000 calls)"
440 SINSUM = 0
450 COSSUM = 0
460 FOR I% = 1 TO 1000
470 SINSUM = SINSUM + SIN(I% * 0.01)
480 COSSUM = COSSUM + COS(I% * 0.01)
490 NEXT I%
500 PRINT "  SIN sum: "; SINSUM
510 PRINT "  COS sum: "; COSSUM
520 PRINT ""
530 REM
540 REM Test 4: Square Root
550 REM
560 PRINT "Test 4: Square root (1000 calls)"
570 SQRSUM = 0
580 FOR I% = 1 TO 1000
590 SQRSUM = SQRSUM + SQR(I%)
600 NEXT I%
610 PRINT "  SQR sum: "; SQRSUM
620 PRINT ""
630 REM
640 REM Test 5: Nested Function Calls
650 REM
660 PRINT "Test 5: Nested functions (1000 calls)"
670 DEF FNCalc(X) = ABS(X * 2 - 10)
680 NESTED = 0
690 FOR I% = 1 TO 1000
700 NESTED = NESTED + FNCalc(I%)
710 NEXT I%
720 PRINT "  Nested sum: "; NESTED
730 PRINT ""
740 REM
750 REM Test 6: Function with Expression
760 REM
770 PRINT "Test 6: Complex function expressions (5000 calls)"
780 DEF FNComplex(X) = (X * 2 + 3) ^ 2 / (X + 1)
790 COMPLEXSUM = 0
800 FOR I% = 1 TO 5000
810 COMPLEXSUM = COMPLEXSUM + FNComplex(I%)
820 NEXT I%
830 PRINT "  Complex sum: "; COMPLEXSUM
840 PRINT ""
850 PRINT "==================================="
860 PRINT "BENCHMARK COMPLETE"
870 PRINT "==================================="
880 END
