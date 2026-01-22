10 REM Arithmetic Performance Benchmark
20 REM Tests raw arithmetic operations speed
30 REM ========================================
40 CLS
50 PRINT "ARITHMETIC PERFORMANCE BENCHMARK"
60 PRINT "================================"
70 PRINT ""
80 REM
90 REM Test 1: Integer Addition
100 REM
110 PRINT "Test 1: Integer addition (10000 iterations)"
120 COUNTER% = 0
130 RESULT% = 0
140 FOR I% = 1 TO 10000
150 RESULT% = RESULT% + 1
160 COUNTER% = COUNTER% + 1
170 NEXT I%
180 PRINT "  Result: "; RESULT%; ", Counter: "; COUNTER%
190 PRINT ""
200 REM
210 REM Test 2: Integer Multiplication
220 REM
230 PRINT "Test 2: Integer multiplication (10000 iterations)"
240 RESULT% = 1
250 FOR I% = 1 TO 10000
260 RESULT% = I% * 2
270 NEXT I%
280 PRINT "  Final result: "; RESULT%
290 PRINT ""
300 REM
310 REM Test 3: Float Operations
320 REM
330 PRINT "Test 3: Float operations (10000 iterations)"
340 SUM! = 0.0
350 FOR I% = 1 TO 10000
360 SUM! = SUM! + 0.1
370 NEXT I%
380 PRINT "  Sum: "; SUM!
390 PRINT ""
400 REM
410 REM Test 4: Mixed Operations
420 REM
430 PRINT "Test 4: Mixed arithmetic (10000 iterations)"
440 A% = 0
450 B! = 0.0
460 C# = 0.0
470 FOR I% = 1 TO 10000
480 A% = A% + 1
490 B! = B! + 0.5
500 C# = C# + A% + B!
510 NEXT I%
520 PRINT "  A%: "; A%; ", B!: "; B!; ", C#: "; C#
530 PRINT ""
540 REM
550 REM Test 5: Division and MOD
560 REM
570 PRINT "Test 5: Division and MOD (5000 iterations)"
580 DIVSUM = 0
590 MODSUM% = 0
600 FOR I% = 1 TO 5000
610 DIVSUM = DIVSUM + (I% / 3)
620 MODSUM% = MODSUM% + (I% MOD 7)
630 NEXT I%
640 PRINT "  Div sum: "; DIVSUM; ", Mod sum: "; MODSUM%
650 PRINT ""
660 REM
670 REM Test 6: Power Operations
680 REM
690 PRINT "Test 6: Power operations (1000 iterations)"
700 POWSUM = 0
710 FOR I% = 1 TO 1000
720 POWSUM = POWSUM + (2 ^ 3)
730 NEXT I%
740 PRINT "  Power sum: "; POWSUM
750 PRINT ""
760 REM
770 REM Test 7: Complex Expression
780 REM
790 PRINT "Test 7: Complex expressions (5000 iterations)"
800 RESULT = 0
810 FOR I% = 1 TO 5000
820 RESULT = (I% * 2 + 3) * (I% - 1) / (I% + 1)
830 NEXT I%
840 PRINT "  Final result: "; RESULT
850 PRINT ""
860 PRINT "================================"
870 PRINT "BENCHMARK COMPLETE"
880 PRINT "================================"
890 END
