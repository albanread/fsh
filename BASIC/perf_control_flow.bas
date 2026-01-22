10 REM Control Flow and Recursion Performance Benchmark
20 REM Tests FOR/NEXT, WHILE/WEND, REPEAT/UNTIL, and recursion
30 REM ===========================================================
40 CLS
50 PRINT "CONTROL FLOW PERFORMANCE BENCHMARK"
60 PRINT "=================================="
70 PRINT ""
80 REM
90 REM Test 1: FOR/NEXT loops
100 REM
110 PRINT "Test 1: FOR/NEXT loops (10000 iterations)"
120 SUM1% = 0
130 FOR I% = 1 TO 10000
140 SUM1% = SUM1% + 1
150 NEXT I%
160 PRINT "  Sum: "; SUM1%
170 PRINT ""
180 REM
190 REM Test 2: Nested FOR loops
200 REM
210 PRINT "Test 2: Nested FOR loops (100x100)"
220 SUM2% = 0
230 FOR I% = 1 TO 100
240 FOR J% = 1 TO 100
250 SUM2% = SUM2% + 1
260 NEXT J%
270 NEXT I%
280 PRINT "  Sum: "; SUM2%
290 PRINT ""
300 REM
310 REM Test 3: FOR loop with STEP
320 REM
330 PRINT "Test 3: FOR with STEP (5000 iterations)"
340 SUM3% = 0
350 FOR I% = 1 TO 10000 STEP 2
360 SUM3% = SUM3% + 1
370 NEXT I%
380 PRINT "  Count: "; SUM3%
390 PRINT ""
400 REM
410 REM Test 4: WHILE/WEND loops
420 REM
430 PRINT "Test 4: WHILE/WEND loops (10000 iterations)"
440 I% = 0
450 SUM4% = 0
460 WHILE I% < 10000
470 I% = I% + 1
480 SUM4% = SUM4% + I%
490 WEND
500 PRINT "  Sum: "; SUM4%
510 PRINT ""
520 REM
530 REM Test 5: Nested WHILE loops
540 REM
550 PRINT "Test 5: Nested WHILE loops (100x100)"
560 I% = 0
570 SUM5% = 0
580 WHILE I% < 100
590 I% = I% + 1
600 J% = 0
610 WHILE J% < 100
620 J% = J% + 1
630 SUM5% = SUM5% + 1
640 WEND
650 WEND
660 PRINT "  Sum: "; SUM5%
670 PRINT ""
680 REM
690 REM Test 6: REPEAT/UNTIL loops
700 REM
710 PRINT "Test 6: REPEAT/UNTIL loops (10000 iterations)"
720 I% = 0
730 SUM6% = 0
740 REPEAT
750 I% = I% + 1
760 SUM6% = SUM6% + I%
770 UNTIL I% >= 10000
780 PRINT "  Sum: "; SUM6%
790 PRINT ""
800 REM
810 REM Test 7: Nested REPEAT/UNTIL
820 REM
830 PRINT "Test 7: Nested REPEAT/UNTIL (100x100)"
840 I% = 0
850 SUM7% = 0
860 REPEAT
870 I% = I% + 1
880 J% = 0
890 REPEAT
900 J% = J% + 1
910 SUM7% = SUM7% + 1
920 UNTIL J% >= 100
930 UNTIL I% >= 100
940 PRINT "  Sum: "; SUM7%
950 PRINT ""
960 REM
970 REM Test 8: Mixed loop types
980 REM
990 PRINT "Test 8: FOR inside WHILE (100x100)"
1000 I% = 0
1010 SUM8% = 0
1020 WHILE I% < 100
1030 I% = I% + 1
1040 FOR J% = 1 TO 100
1050 SUM8% = SUM8% + 1
1060 NEXT J%
1070 WEND
1080 PRINT "  Sum: "; SUM8%
1090 PRINT ""
1100 REM
1110 REM Test 9: Simple recursion (factorial)
1120 REM
1130 PRINT "Test 9: Factorial recursion"
1140 F1 = Fact(5)
1150 F2 = Fact(10)
1160 F3 = Fact(12)
1170 PRINT "  Fact(5): "; F1
1180 PRINT "  Fact(10): "; F2
1190 PRINT "  Fact(12): "; F3
1200 PRINT ""
1210 REM
1220 REM Test 10: Fibonacci recursion
1230 REM
1240 PRINT "Test 10: Fibonacci recursion"
1250 FIB1 = Fib(10)
1260 FIB2 = Fib(15)
1270 FIB3 = Fib(20)
1280 PRINT "  Fib(10): "; FIB1
1290 PRINT "  Fib(15): "; FIB2
1300 PRINT "  Fib(20): "; FIB3
1310 PRINT ""
1320 REM
1330 REM Test 11: Iterative factorial (for comparison)
1340 REM
1350 PRINT "Test 11: Iterative factorial (1000 calls)"
1360 FOR I% = 1 TO 1000
1370 R = IterFact(10)
1380 NEXT I%
1390 PRINT "  IterFact(10): "; R
1400 PRINT ""
1410 REM
1420 REM Test 12: Loop with early exit (IF/THEN)
1430 REM
1440 PRINT "Test 12: Loop with conditional exit (10000 iterations)"
1450 COUNT% = 0
1460 FOR I% = 1 TO 10000
1470 IF I% > 5000 THEN GOTO 1500
1480 COUNT% = COUNT% + 1
1490 NEXT I%
1500 PRINT "  Count: "; COUNT%
1510 PRINT ""
1520 REM
1530 REM Test 13: Loop performance comparison
1540 REM
1550 PRINT "Test 13: Loop type comparison (5000 each)"
1560 REM FOR loop
1570 S1% = 0
1580 FOR I% = 1 TO 5000
1590 S1% = S1% + 1
1600 NEXT I%
1610 REM WHILE loop
1620 S2% = 0
1630 I% = 0
1640 WHILE I% < 5000
1650 I% = I% + 1
1660 S2% = S2% + 1
1670 WEND
1680 REM REPEAT loop
1690 S3% = 0
1700 I% = 0
1710 REPEAT
1720 I% = I% + 1
1730 S3% = S3% + 1
1740 UNTIL I% >= 5000
1750 PRINT "  FOR: "; S1%; ", WHILE: "; S2%; ", REPEAT: "; S3%
1760 PRINT ""
1770 REM
1780 REM Test 14: Deep recursion test
1790 REM
1800 PRINT "Test 14: Recursion depth test"
1810 D1 = Depth(5)
1820 D2 = Depth(10)
1830 D3 = Depth(15)
1840 PRINT "  Depth(5): "; D1
1850 PRINT "  Depth(10): "; D2
1860 PRINT "  Depth(15): "; D3
1870 PRINT ""
1880 PRINT "=================================="
1890 PRINT "BENCHMARK COMPLETE"
1900 PRINT "=================================="
1910 END
1920 REM
1930 REM Function definitions
1940 REM
1950 FUNCTION Fact(N)
1960 IF N <= 1 THEN Fact = 1 ELSE Fact = N * Fact(N - 1)
1970 ENDFUNCTION
1980 REM
1990 FUNCTION Fib(N)
2000 IF N <= 1 THEN Fib = N ELSE Fib = Fib(N - 1) + Fib(N - 2)
2010 ENDFUNCTION
2020 REM
2030 FUNCTION IterFact(N)
2040 LOCAL RESULT
2050 LOCAL I
2060 RESULT = 1
2070 FOR I = 1 TO N
2080 RESULT = RESULT * I
2090 NEXT I
2100 IterFact = RESULT
2110 ENDFUNCTION
2120 REM
2130 FUNCTION Depth(N)
2140 IF N <= 0 THEN Depth = 0 ELSE Depth = 1 + Depth(N - 1)
2150 ENDFUNCTION
