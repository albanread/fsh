10 REM Variable and Array Performance Benchmark
20 REM Tests variable access and array operations
30 REM ==============================================
40 CLS
50 PRINT "VARIABLE/ARRAY PERFORMANCE BENCHMARK"
60 PRINT "===================================="
70 PRINT ""
80 REM
90 REM Test 1: Integer Variable Assignment
100 REM
110 PRINT "Test 1: Integer variable assignment (10000 iterations)"
120 A% = 0
130 FOR I% = 1 TO 10000
140 A% = I%
150 NEXT I%
160 PRINT "  Final value: "; A%
170 PRINT ""
180 REM
190 REM Test 2: Multiple Variable Types
200 REM
210 PRINT "Test 2: Mixed type variables (5000 iterations)"
220 INT_VAR% = 0
230 FLOAT_VAR! = 0.0
240 DOUBLE_VAR# = 0.0
250 STRING_VAR$ = ""
260 FOR I% = 1 TO 5000
270 INT_VAR% = I%
280 FLOAT_VAR! = I% * 1.5
290 DOUBLE_VAR# = I% * 2.5
300 STRING_VAR$ = "X"
310 NEXT I%
320 PRINT "  Int: "; INT_VAR%; ", Float: "; FLOAT_VAR!
330 PRINT "  Double: "; DOUBLE_VAR#; ", String: "; STRING_VAR$
340 PRINT ""
350 REM
360 REM Test 3: Long Variable Names
370 REM
380 PRINT "Test 3: Long variable names (5000 iterations)"
390 PlayerScore% = 0
400 EnemyHealthPoints% = 100
410 TotalDamageDealt! = 0.0
420 FOR I% = 1 TO 5000
430 PlayerScore% = PlayerScore% + 10
440 EnemyHealthPoints% = EnemyHealthPoints% - 1
450 TotalDamageDealt! = TotalDamageDealt! + 0.5
460 NEXT I%
470 PRINT "  PlayerScore%: "; PlayerScore%
480 PRINT "  EnemyHealthPoints%: "; EnemyHealthPoints%
490 PRINT "  TotalDamageDealt!: "; TotalDamageDealt!
500 PRINT ""
510 REM
520 REM Test 4: Array Creation and Access
530 REM
540 PRINT "Test 4: Array operations (1000 elements)"
550 DIM NUMS(1000)
560 DIM STRINGS$(100)
570 FOR I% = 0 TO 999
580 NUMS(I%) = I% * 2
590 NEXT I%
600 FOR I% = 0 TO 99
610 STRINGS$(I%) = "TEST"
620 NEXT I%
630 PRINT "  Array initialized"
640 PRINT "  NUMS(500): "; NUMS(500)
650 PRINT "  STRINGS$(50): "; STRINGS$(50)
660 PRINT ""
670 REM
680 REM Test 5: Array Sum Calculation
690 REM
700 PRINT "Test 5: Array sum (1000 elements)"
710 DIM VALUES(1000)
720 FOR I% = 0 TO 999
730 VALUES(I%) = I%
740 NEXT I%
750 SUM% = 0
760 FOR I% = 0 TO 999
770 SUM% = SUM% + VALUES(I%)
780 NEXT I%
790 PRINT "  Sum of 0-999: "; SUM%
800 PRINT ""
810 REM
820 REM Test 6: Variable Scope (Local vs Global)
830 REM
840 PRINT "Test 6: Variable read/write (10000 iterations)"
850 GLOBAL_VAR% = 0
860 LOCAL_VAR% = 0
870 FOR I% = 1 TO 10000
880 GLOBAL_VAR% = GLOBAL_VAR% + 1
890 LOCAL_VAR% = LOCAL_VAR% + 1
900 NEXT I%
910 PRINT "  Global: "; GLOBAL_VAR%; ", Local: "; LOCAL_VAR%
920 PRINT ""
930 REM
940 REM Test 7: String Concatenation
950 REM
960 PRINT "Test 7: String operations (1000 iterations)"
970 RESULT$ = ""
980 COUNT% = 0
990 FOR I% = 1 TO 1000
1000 TMP$ = "X"
1010 COUNT% = COUNT% + 1
1020 NEXT I%
1030 PRINT "  Iterations: "; COUNT%
1040 PRINT ""
1050 REM
1060 REM Test 8: Array with Expressions
1070 REM
1080 PRINT "Test 8: Array access with expressions (1000 iterations)"
1090 DIM DATA(100)
1100 FOR I% = 0 TO 99
1110 DATA(I%) = I% * I%
1120 NEXT I%
1130 SUM2% = 0
1140 FOR I% = 0 TO 99
1150 IDX% = I%
1160 SUM2% = SUM2% + DATA(IDX%)
1170 NEXT I%
1180 PRINT "  Sum of squares 0-99: "; SUM2%
1190 PRINT ""
1200 REM
1210 REM Test 9: Type Conversion Performance
1220 REM
1230 PRINT "Test 9: Type conversions (5000 iterations)"
1240 INT_RESULT% = 0
1250 FLOAT_INPUT! = 0.0
1260 FOR I% = 1 TO 5000
1270 FLOAT_INPUT! = I% * 1.7
1280 INT_RESULT% = FLOAT_INPUT!
1290 NEXT I%
1300 PRINT "  Converted: "; INT_RESULT%
1310 PRINT ""
1320 PRINT "===================================="
1330 PRINT "BENCHMARK COMPLETE"
1340 PRINT "===================================="
1350 END
