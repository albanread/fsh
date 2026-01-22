' Array Operations Performance Test
' Tests array initialization, access, and manipulation

PRINT "Array Operations Performance Test"
PRINT "=================================="
PRINT ""

' Test 1: Array initialization and sum
PRINT "Test 1: Array Initialization and Sum"
DIM arr(100)
DIM i
DIM j
DIM sum

PRINT "Initializing and summing array of 100 elements, 1000 times..."
FOR j = 1 TO 1000
    sum = 0
    FOR i = 0 TO 99
        arr(i) = i * 1.5
    NEXT i
    FOR i = 0 TO 99
        sum = sum + arr(i)
    NEXT i
NEXT j
PRINT "Sum: "; sum
PRINT ""

' Test 2: Array reversal
PRINT "Test 2: Array Reversal"
DIM temp
DIM left
DIM right

' Initialize array
FOR i = 0 TO 99
    arr(i) = i
NEXT i

PRINT "Reversing array of 100 elements, 1000 times..."
FOR j = 1 TO 1000
    left = 0
    right = 99
    WHILE left < right
        temp = arr(left)
        arr(left) = arr(right)
        arr(right) = temp
        left = left + 1
        right = right - 1
    WEND
NEXT j
PRINT "First element after reversals: "; arr(0)
PRINT "Last element after reversals: "; arr(99)
PRINT ""

' Test 3: Array maximum/minimum
PRINT "Test 3: Find Array Maximum and Minimum"
DIM max
DIM min

' Initialize with some values
FOR i = 0 TO 99
    arr(i) = (i * 7) MOD 100
NEXT i

PRINT "Finding max/min in array of 100 elements, 1000 times..."
FOR j = 1 TO 1000
    max = arr(0)
    min = arr(0)
    FOR i = 1 TO 99
        IF arr(i) > max THEN
            max = arr(i)
        END IF
        IF arr(i) < min THEN
            min = arr(i)
        END IF
    NEXT i
NEXT j
PRINT "Maximum: "; max
PRINT "Minimum: "; min
PRINT ""

' Test 4: Array copy
PRINT "Test 4: Array Copy"
DIM dest(100)

PRINT "Copying array of 100 elements, 5000 times..."
FOR j = 1 TO 5000
    FOR i = 0 TO 99
        dest(i) = arr(i)
    NEXT i
NEXT j
PRINT "First element: "; dest(0)
PRINT "Last element: "; dest(99)
PRINT ""

' Test 5: Array multiply and accumulate
PRINT "Test 5: Array Multiply and Accumulate"
DIM result

' Initialize arrays
FOR i = 0 TO 99
    arr(i) = i + 1
    dest(i) = 100 - i
NEXT i

PRINT "Dot product of two 100-element arrays, 1000 times..."
FOR j = 1 TO 1000
    result = 0
    FOR i = 0 TO 99
        result = result + arr(i) * dest(i)
    NEXT i
NEXT j
PRINT "Dot product: "; result
PRINT ""

PRINT "Performance test complete!"
