' Mathematical Operations Performance Test
' Tests various mathematical computations and trigonometric functions

PRINT "Mathematical Operations Performance Test"
PRINT "========================================"
PRINT ""

' Test 1: Basic arithmetic operations
PRINT "Test 1: Basic Arithmetic Operations"
LET a = 0
LET b = 0
LET result = 0
LET i = 0

a = 123.456
b = 78.9

PRINT "Performing 50000 mixed arithmetic operations..."
FOR i = 1 TO 50000
    result = a + b
    result = result - 10.5
    result = result * 2.0
    result = result / 3.0
NEXT i
PRINT "Final result: "; result
PRINT ""

' Test 2: Trigonometric functions
PRINT "Test 2: Trigonometric Functions"
LET angle = 0
LET sinVal = 0
LET cosVal = 0
LET tanVal = 0

PRINT "Computing sin, cos, tan for 10000 angles..."
FOR i = 1 TO 10000
    angle = i * 0.01
    sinVal = SIN(angle)
    cosVal = COS(angle)
    tanVal = TAN(angle)
NEXT i
PRINT "Last sin: "; sinVal
PRINT "Last cos: "; cosVal
PRINT "Last tan: "; tanVal
PRINT ""

' Test 3: Exponential and logarithmic functions
PRINT "Test 3: Exponential and Logarithmic Functions"
LET expVal = 0
LET logVal = 0
LET x = 0

PRINT "Computing exp and log for 10000 values..."
FOR i = 1 TO 10000
    x = 1.0 + i * 0.001
    expVal = EXP(x)
    logVal = LOG(x)
NEXT i
PRINT "Last exp: "; expVal
PRINT "Last log: "; logVal
PRINT ""

' Test 4: Square root operations
PRINT "Test 4: Square Root Operations"
LET sqrtVal = 0

PRINT "Computing square roots for 50000 values..."
FOR i = 1 TO 50000
    x = i * 0.1
    sqrtVal = SQR(x)
NEXT i
PRINT "Last sqrt: "; sqrtVal
PRINT ""

' Test 5: Power operations
PRINT "Test 5: Power Operations"
LET powVal = 0

PRINT "Computing powers (x^2.5) for 10000 values..."
FOR i = 1 TO 10000
    x = 1.0 + i * 0.01
    powVal = x ^ 2.5
NEXT i
PRINT "Last power: "; powVal
PRINT ""

' Test 6: Integer operations
PRINT "Test 6: Integer Operations (MOD and division)"
LET intA = 0
LET intB = 0
LET modResult = 0
LET divResult = 0

intA = 12345
intB = 67

PRINT "Performing 50000 MOD and integer division operations..."
FOR i = 1 TO 50000
    modResult = intA MOD intB
    divResult = intA \ intB
    intA = intA + 1
    IF intA > 99999 THEN
        intA = 10000
    END IF
NEXT i
PRINT "Last MOD result: "; modResult
PRINT "Last DIV result: "; divResult
PRINT ""

' Test 7: ABS and SGN operations
PRINT "Test 7: ABS and SGN Operations"
LET absVal = 0
LET sgnVal = 0
LET valNum = 0

PRINT "Computing ABS and SGN for 50000 values..."
FOR i = 1 TO 50000
    valNum = SIN(i * 0.1) * 100.0
    absVal = ABS(valNum)
    sgnVal = SGN(valNum)
NEXT i
PRINT "Last ABS: "; absVal
PRINT "Last SGN: "; sgnVal
PRINT ""

' Test 8: Combined mathematical expression
PRINT "Test 8: Complex Mathematical Expression"
LET complex = 0

PRINT "Evaluating complex expression 10000 times..."
PRINT "Expression: sqrt(sin(x)^2 + cos(x)^2) * exp(x/100)"
FOR i = 1 TO 10000
    x = i * 0.01
    complex = SQR(SIN(x) ^ 2 + COS(x) ^ 2) * EXP(x / 100.0)
NEXT i
PRINT "Last result: "; complex
PRINT ""

PRINT "Performance test complete!"
