REM Performance Test: GOTO and GOSUB Heavy Flow Control
REM Tests the performance of GOTO and GOSUB statements
REM which currently use linear scanning to find target lines

10 PRINT "GOTO/GOSUB Performance Test"
20 PRINT "=============================="
30 PRINT ""

REM Test 1: GOTO in a tight loop
40 PRINT "Test 1: GOTO tight loop (10000 iterations)"
50 I = 0
60 GOTO 1000

REM Test 2: GOTO with forward and backward jumps
100 PRINT "Test 2: GOTO forward/backward jumps (5000 iterations)"
110 J = 0
120 GOTO 2000

REM Test 3: GOSUB repeated calls
200 PRINT "Test 3: GOSUB repeated calls (10000 iterations)"
210 K = 0
220 GOTO 3000

REM Test 4: Nested GOSUB calls
300 PRINT "Test 4: Nested GOSUB (5000 iterations)"
310 M = 0
320 GOTO 4000

REM Test 5: Mixed GOTO and GOSUB
400 PRINT "Test 5: Mixed GOTO/GOSUB (5000 iterations)"
410 N = 0
420 GOTO 5000

REM Final results
500 PRINT ""
510 PRINT "All tests complete!"
520 PRINT "Total iterations: 35000"
530 END

REM ============================================
REM Test 1: GOTO tight loop
REM ============================================
1000 I = I + 1
1010 IF I >= 10000 THEN GOTO 1100
1020 GOTO 1000
1100 PRINT "  Completed: "; I; " iterations"
1110 GOTO 100

REM ============================================
REM Test 2: GOTO forward/backward jumps
REM ============================================
2000 J = J + 1
2010 IF J >= 5000 THEN GOTO 2200
2020 REM Forward jump to 2100
2030 GOTO 2100
2100 REM Do some work
2110 X = J * 2
2120 X = X + 1
2130 REM Backward jump to 2000
2140 GOTO 2000
2200 PRINT "  Completed: "; J; " iterations"
2210 GOTO 200

REM ============================================
REM Test 3: GOSUB repeated calls
REM ============================================
3000 K = K + 1
3010 IF K >= 10000 THEN GOTO 3100
3020 GOSUB 3500
3030 GOTO 3000
3100 PRINT "  Completed: "; K; " iterations"
3110 GOTO 300

REM Subroutine for Test 3
3500 Y = K * 3
3510 Y = Y + K
3520 RETURN

REM ============================================
REM Test 4: Nested GOSUB calls
REM ============================================
4000 M = M + 1
4010 IF M >= 5000 THEN GOTO 4100
4020 GOSUB 4500
4030 GOTO 4000
4100 PRINT "  Completed: "; M; " iterations"
4110 GOTO 400

REM Level 1 subroutine
4500 Z = M * 2
4510 GOSUB 4600
4520 RETURN

REM Level 2 subroutine
4600 Z = Z + M
4610 GOSUB 4700
4620 RETURN

REM Level 3 subroutine
4700 Z = Z * 2
4710 RETURN

REM ============================================
REM Test 5: Mixed GOTO and GOSUB
REM ============================================
5000 N = N + 1
5010 IF N >= 5000 THEN GOTO 5300
5020 REM Alternate between GOTO and GOSUB
5030 IF N MOD 2 = 0 THEN GOTO 5100
5040 GOSUB 5500
5050 GOTO 5000
5100 REM GOTO path
5110 W = N * 4
5120 GOTO 5200
5200 W = W + N
5210 GOTO 5000
5300 PRINT "  Completed: "; N; " iterations"
5310 GOTO 500

REM Subroutine for Test 5
5500 W = N * 5
5510 W = W - N
5520 RETURN
