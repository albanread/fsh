REM =========================================
REM Sieve of Eratosthenes
REM Classic prime number finding algorithm
REM ==========================================
REM Configuration
CONSTANT limit = 8192
PRINT "Sieve of Eratosthenes"
PRINT "Searching for primes up to "; limit
PRINT
REM Initialize array - 1 means prime, 0 means not prime
DIM flags(limit)
FOR i = 2 TO limit
  flags(i) = 1
NEXT i
REM standard basic Sieve algorithm
FOR i = 2 TO limit
  IF flags(i) = 1 THEN
    REM Mark multiples as non-prime using STEP
    FOR j = i + i TO limit STEP i
      flags(j) = 0
    NEXT j
  END IF
NEXT i
REM Count primes
count = 0
FOR i = 2 TO limit
  IF flags(i) = 1 THEN
    count = count + 1
  END IF
NEXT i
PRINT "Found "; count; " prime numbers"
PRINT
REM Show first 20 primes
PRINT "First 20 primes:"
found = 0
FOR i = 2 TO limit
  IF flags(i) = 1 THEN
    PRINT i; " ";
    found = found + 1
    IF found = 20 THEN
      EXIT FOR
    END IF
  END IF
NEXT i
PRINT
PRINT
PRINT "Complete!"