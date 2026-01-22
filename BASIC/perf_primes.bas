' Prime Numbers Performance Test
' Finds prime numbers using trial division

FUNCTION IsPrime(n)
    IF n <= 1 THEN
        RETURN 0
    END IF
    IF n <= 3 THEN
        RETURN 1
    END IF
    IF n MOD 2 = 0 THEN
        RETURN 0
    END IF

    i = 3
    WHILE i * i <= n
        IF n MOD i = 0 THEN
            RETURN 0
        END IF
        i = i + 2
    WEND
    RETURN 1
END FUNCTION

FUNCTION CountPrimes(limit)
    count = 0
    FOR i = 2 TO limit
        IF IsPrime(i) = 1 THEN
            count = count + 1
        END IF
    NEXT i
    RETURN count
END FUNCTION

FUNCTION FindNthPrime(n)
    IF n = 1 THEN
        RETURN 2
    END IF

    count = 1
    candidate = 3

    WHILE count < n
        IF IsPrime(candidate) = 1 THEN
            count = count + 1
        END IF
        IF count < n THEN
            candidate = candidate + 2
        END IF
    WEND

    RETURN candidate
END FUNCTION

FUNCTION SumPrimes(limit)
    sum = 0
    FOR i = 2 TO limit
        IF IsPrime(i) = 1 THEN
            sum = sum + i
        END IF
    NEXT i
    RETURN sum
END FUNCTION

PRINT "Prime Numbers Performance Test"
PRINT "=============================="
PRINT ""

' Test 1: Count primes up to N using trial division
PRINT "Test 1: Trial Division Method"

' Run prime counting test
PRINT "Counting primes up to 1000, 100 times..."
FOR i = 1 TO 100
    result = CountPrimes(1000)
NEXT i
PRINT "Number of primes up to 1000: "; result
PRINT ""

' Test 2: Find Nth prime
PRINT "Test 2: Find Nth Prime Number"

PRINT "Finding 100th prime number 100 times..."
FOR i = 1 TO 100
    result = FindNthPrime(100)
NEXT i
PRINT "The 100th prime is: "; result
PRINT ""

' Test 3: Sum of primes
PRINT "Test 3: Sum of Primes"

PRINT "Summing primes up to 500, 100 times..."
FOR i = 1 TO 100
    sumResult = SumPrimes(500)
NEXT i
PRINT "Sum of primes up to 500: "; sumResult
PRINT ""

PRINT "Performance test complete!"
