' Factorial Performance Test
' Calculates factorial iteratively and recursively

FUNCTION IterativeFactorial(n)
    result = 1
    FOR i = 2 TO n
        result = result * i
    NEXT i
    RETURN result
END FUNCTION

FUNCTION RecursiveFactorial(n)
    IF n <= 1 THEN
        RETURN 1
    ELSE
        RETURN n * RecursiveFactorial(n - 1)
    END IF
END FUNCTION

PRINT "Factorial Performance Test"
PRINT "=========================="
PRINT ""

' Test 1: Iterative factorial
PRINT "Test 1: Iterative Factorial"
n = 20

' Warmup
result = IterativeFactorial(10)

' Run iterative test multiple times
PRINT "Computing factorial(20) iteratively 10000 times..."
FOR i = 1 TO 10000
    result = IterativeFactorial(n)
NEXT i
PRINT "Result: "; result
PRINT ""

' Test 2: Recursive factorial (fewer iterations due to overhead)
PRINT "Test 2: Recursive Factorial"
PRINT "Computing factorial(20) recursively 1000 times..."
FOR i = 1 TO 1000
    result = RecursiveFactorial(n)
NEXT i
PRINT "Result: "; result
PRINT ""

' Test 3: Direct computation in loop
PRINT "Test 3: Inline Factorial Computation"
PRINT "Computing factorial(20) inline 10000 times..."
FOR i = 1 TO 10000
    result = 1
    FOR j = 2 TO n
        result = result * j
    NEXT j
NEXT i
PRINT "Result: "; result
PRINT ""

PRINT "Performance test complete!"
