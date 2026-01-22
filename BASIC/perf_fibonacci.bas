' Fibonacci Performance Test
' Calculates Fibonacci numbers using different methods

FUNCTION IterativeFib(n)
    IF n <= 1 THEN
        RETURN n
    END IF

    a = 0
    b = 1
    FOR i = 2 TO n
        temp = a + b
        a = b
        b = temp
    NEXT i
    RETURN b
END FUNCTION

FUNCTION RecursiveFib(n)
    IF n <= 1 THEN
        RETURN n
    ELSE
        RETURN RecursiveFib(n - 1) + RecursiveFib(n - 2)
    END IF
END FUNCTION

PRINT "Fibonacci Performance Test"
PRINT "=========================="
PRINT ""

' Test 1: Iterative Fibonacci
PRINT "Test 1: Iterative Fibonacci"

' Warmup
result = IterativeFib(20)

' Run iterative test
PRINT "Computing fib(30) iteratively 10000 times..."
FOR i = 1 TO 10000
    result = IterativeFib(30)
NEXT i
PRINT "Result: "; result
PRINT ""

' Test 2: Recursive (much fewer iterations)
PRINT "Test 2: Recursive Fibonacci"
PRINT "Computing fib(20) recursively 100 times..."
FOR i = 1 TO 100
    result = RecursiveFib(20)
NEXT i
PRINT "Result: "; result
PRINT ""

' Test 3: Inline iterative computation
PRINT "Test 3: Inline Fibonacci Computation"
PRINT "Computing fib(30) inline 10000 times..."
FOR i = 1 TO 10000
    a = 0
    b = 1
    FOR j = 2 TO 30
        temp = a + b
        a = b
        b = temp
    NEXT j
    result = b
NEXT i
PRINT "Result: "; result
PRINT ""

PRINT "Performance test complete!"
