REM Tight GOTO Loop Performance Test
REM Pure GOTO performance with minimal other operations
REM This test isolates GOTO overhead by doing almost nothing else

10 PRINT "Tight GOTO Loop Test"
20 PRINT "====================="
30 PRINT ""
40 PRINT "Testing pure GOTO performance with 50000 iterations"
50 PRINT ""

REM Initialize counter
100 COUNT = 0

REM Tight loop - just GOTO and counter increment
200 COUNT = COUNT + 1
210 IF COUNT >= 50000 THEN GOTO 300
220 GOTO 200

REM Done
300 PRINT "Completed "; COUNT; " iterations"
310 PRINT "Each iteration: 1 increment, 1 comparison, 2 GOTOs"
320 PRINT "Total GOTO statements: "; COUNT * 2
330 END
