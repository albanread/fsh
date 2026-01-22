' Factorial Performance Test (Classic GOTO/GOSUB Style)
' Calculates factorial using classic BASIC control flow

' Main program
10 PRINT "Factorial Performance Test (GOTO/GOSUB)"
20 PRINT "=========================================="
30 PRINT ""

' Test 1: Iterative factorial with GOTO loops
40 PRINT "Test 1: Iterative Factorial"
50 N = 20
60 WARMUP = 1000
70 ITERATIONS = 10000

' Warmup run
80 TESTVAL = 10
90 GOSUB 1000
100 PRINT "Warmup result: "; RESULT

' Main test
110 PRINT "Running iterative test..."
120 I = 0
130 I = I + 1
140 IF I > ITERATIONS THEN GOTO 170
150 GOSUB 1000
160 GOTO 130
170 PRINT "Completed "; ITERATIONS; " iterations"
180 PRINT "Final result: "; RESULT
190 PRINT ""

' Test 2: Factorial with GOSUB (fewer iterations due to call overhead)
200 PRINT "Test 2: GOSUB Factorial"
210 N = 15
220 ITERATIONS2 = 1000
230 I = 0
240 I = I + 1
250 IF I > ITERATIONS2 THEN GOTO 280
260 GOSUB 2000
270 GOTO 240
280 PRINT "Completed "; ITERATIONS2; " iterations"
290 PRINT "Final result: "; RESULT
300 PRINT ""

' Test 3: Direct computation in loop
310 PRINT "Test 3: Direct Computation Loop"
320 N = 20
330 ITERATIONS3 = 10000
340 I = 0
350 I = I + 1
360 IF I > ITERATIONS3 THEN GOTO 450
370 ' Inline factorial calculation
380 RESULT = 1
390 J = 2
400 IF J > N THEN GOTO 430
410 RESULT = RESULT * J
420 J = J + 1
430 IF J <= N THEN GOTO 410
440 GOTO 350
450 PRINT "Completed "; ITERATIONS3; " iterations"
460 PRINT "Final result: "; RESULT
470 PRINT ""

480 PRINT "Performance test complete!"
490 END

' Subroutine: Iterative factorial (N -> RESULT)
1000 RESULT = 1
1010 J = 2
1020 IF J > N THEN GOTO 1050
1030 RESULT = RESULT * J
1040 J = J + 1
1050 IF J <= N THEN GOTO 1030
1060 RETURN

' Subroutine: Recursive-style factorial using GOSUB
' (Simulated recursion with manual stack management)
2000 RESULT = 1
2010 K = N
2020 IF K <= 1 THEN GOTO 2060
2030 RESULT = RESULT * K
2040 K = K - 1
2050 GOTO 2020
2060 RETURN
