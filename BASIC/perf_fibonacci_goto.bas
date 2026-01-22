' Fibonacci Performance Test (Classic GOTO/GOSUB Style)
' Calculates Fibonacci numbers using classic BASIC control flow

10 PRINT "Fibonacci Performance Test (GOTO/GOSUB)"
20 PRINT "=========================================="
30 PRINT ""

' Test 1: Iterative Fibonacci with GOSUB
40 PRINT "Test 1: Iterative Fibonacci"
50 N = 30
60 ITERATIONS = 5000

' Warmup run
70 N = 10
80 GOSUB 1000
90 PRINT "Warmup result: "; RESULT

' Main test
100 N = 30
110 PRINT "Running iterative test..."
120 I = 0
130 I = I + 1
140 IF I > ITERATIONS THEN GOTO 170
150 GOSUB 1000
160 GOTO 130
170 PRINT "Completed "; ITERATIONS; " iterations"
180 PRINT "Final result: "; RESULT
190 PRINT ""

' Test 2: Inline loop
200 PRINT "Test 2: Inline Fibonacci Loop"
210 N = 30
220 ITERATIONS2 = 5000
230 I = 0
240 I = I + 1
250 IF I > ITERATIONS2 THEN GOTO 370
260 IF N <= 1 THEN GOTO 350
270 A = 0
280 B = 1
290 J = 2
300 TEMP = A + B
310 A = B
320 B = TEMP
330 J = J + 1
340 IF J <= N THEN GOTO 300
350 RESULT = B
360 GOTO 240
370 PRINT "Completed "; ITERATIONS2; " iterations"
380 PRINT "Final result: "; RESULT
390 PRINT ""

' Test 3: GOSUB version with fewer iterations
400 PRINT "Test 3: GOSUB Fibonacci"
410 N = 25
420 ITERATIONS3 = 1000
430 I = 0
440 I = I + 1
450 IF I > ITERATIONS3 THEN GOTO 480
460 GOSUB 2000
470 GOTO 440
480 PRINT "Completed "; ITERATIONS3; " iterations"
490 PRINT "Final result: "; RESULT
500 PRINT ""

510 PRINT "Performance test complete!"
520 END

' Subroutine: Iterative Fibonacci (N -> RESULT)
1000 IF N <= 1 THEN RESULT = N: GOTO 1110
1010 A = 0
1020 B = 1
1030 J = 2
1040 TEMP = A + B
1050 A = B
1060 B = TEMP
1070 J = J + 1
1080 IF J <= N THEN GOTO 1040
1090 RESULT = B
1110 RETURN

' Subroutine: Fast Fibonacci calculation
2000 IF N <= 1 THEN RESULT = N: GOTO 2100
2010 PREV2 = 0
2020 PREV1 = 1
2030 K = 2
2040 CURRENT = PREV1 + PREV2
2050 PREV2 = PREV1
2060 PREV1 = CURRENT
2070 K = K + 1
2080 IF K <= N THEN GOTO 2040
2090 RESULT = CURRENT
2100 RETURN
