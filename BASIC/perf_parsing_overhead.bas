REM Performance Test: Parsing Overhead
REM This test measures the overhead of re-parsing statements on every execution
REM Uses tight loops to amplify parsing cost

10 PRINT "Parsing Overhead Test"
20 PRINT "====================="
30 PRINT ""
40 PRINT "Testing parsing overhead with complex expressions"
50 PRINT ""

REM Test 1: Simple statement in tight loop
60 PRINT "Test 1: Simple assignment (100000 iterations)"
70 FOR I = 1 TO 100000
80   X = 5
90 NEXT I
100 PRINT "  Done: X = "; X
110 PRINT ""

REM Test 2: Complex expression in tight loop
120 PRINT "Test 2: Complex expression (50000 iterations)"
130 FOR I = 1 TO 50000
140   Y = (I + 5) * 2 - 3 / 2 + (I MOD 10)
150 NEXT I
160 PRINT "  Done: Y = "; Y
170 PRINT ""

REM Test 3: Multiple operations per line
180 PRINT "Test 3: Multiple operations (25000 iterations)"
190 FOR I = 1 TO 25000
200   Z = I * 2 : W = Z + 1 : V = W * 3
210 NEXT I
220 PRINT "  Done: Z = "; Z; ", W = "; W; ", V = "; V
230 PRINT ""

REM Test 4: String operations
240 PRINT "Test 4: String operations (10000 iterations)"
250 A$ = "Hello"
260 FOR I = 1 TO 10000
270   B$ = A$ + " World"
280 NEXT I
290 PRINT "  Done: B$ = "; B$
300 PRINT ""

REM Test 5: Nested expressions with functions
310 PRINT "Test 5: Function calls (5000 iterations)"
320 FOR I = 1 TO 5000
330   R = ABS(I - 2500) + INT(I / 100) + SQR(I MOD 100 + 1)
340 NEXT I
350 PRINT "  Done: R = "; R
360 PRINT ""

REM Test 6: Array access with expression index
370 PRINT "Test 6: Array access (5000 iterations)"
380 DIM ARR(100)
390 FOR I = 1 TO 5000
400   ARR(I MOD 100) = I * 2
410   T = ARR((I + 5) MOD 100)
420 NEXT I
430 PRINT "  Done: T = "; T
440 PRINT ""

450 PRINT "All tests complete!"
460 PRINT "Each iteration re-parses the entire statement"
470 END
