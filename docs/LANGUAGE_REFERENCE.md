# FasterBASIC Language Reference Manual

Version 1.0.0

## Table of Contents

1. [Introduction](#introduction)
2. [Program Structure](#program-structure)
3. [Data Types](#data-types)
4. [Variables and Constants](#variables-and-constants)
5. [Operators](#operators)
6. [Control Flow](#control-flow)
7. [Loops](#loops)
8. [Subroutines and Functions](#subroutines-and-functions)
9. [Arrays](#arrays)
10. [User-Defined Types](#user-defined-types)
11. [Input/Output](#inputoutput)
12. [File I/O](#file-io)
13. [Built-in Functions](#built-in-functions)
14. [Compiler Directives](#compiler-directives)
15. [Graphics and Sprites](#graphics-and-sprites)
16. [Event System and Timers](#event-system-and-timers)
17. [Plugin System](#plugin-system)

---

## Introduction

FasterBASIC is a modern BASIC dialect designed for high performance through LuaJIT compilation. It maintains compatibility with classic BASIC while adding modern features like user-defined types, Unicode support, and an event-driven programming model.

### Key Features

- **Classic BASIC syntax** with line numbers (optional)
- **Modern structured programming** (IF/ENDIF, WHILE/WEND, etc.)
- **Strong type system** with user-defined types
- **High performance** via LuaJIT JIT compilation
- **Unicode string support**
- **Event-driven programming** with timers
- **Graphics and sprite system**
- **Extensible plugin architecture**

---

## Program Structure

### Line Numbers

Line numbers are optional in FasterBASIC. Programs can use either:

**Traditional numbered style:**
```basic
10 PRINT "Hello"
20 FOR I = 1 TO 10
30   PRINT I
40 NEXT I
50 END
```

**Modern unnumbered style:**
```basic
PRINT "Hello"
FOR I = 1 TO 10
  PRINT I
NEXT I
END
```

**Mixed style:**
```basic
PRINT "Starting..."
100 FOR I = 1 TO 10
    PRINT I
NEXT I
PRINT "Done"
```

### Comments

```basic
REM This is a comment
PRINT "Hello"  REM Comments can follow statements
```

### Statement Separators

Multiple statements on one line can be separated by colons:

```basic
10 X = 5 : Y = 10 : PRINT X + Y
```

---

## Data Types

### Numeric Types


| Type | Suffix | Description | Range |
|------|--------|-------------|-------|
| INTEGER | % | 32-bit signed integer | -2,147,483,648 to 2,147,483,647 |
| SINGLE | ! | Single precision float | ±3.4E±38 (7 digits) |
| DOUBLE | # | Double precision float | ±1.7E±308 (15 digits) |
| LONG | (none) | Alias for INTEGER | Same as INTEGER |

### String Type

| Type | Suffix | Description |
|------|--------|-------------|
| STRING | $ | Variable-length text | Up to available memory |

### Type Suffixes

Variables can be typed using suffixes:

```basic
X% = 42         REM Integer
Y! = 3.14       REM Single precision
Z# = 3.14159265 REM Double precision
NAME$ = "John"  REM String
```

### Explicit Type Declaration

```basic
DIM Age AS INTEGER
DIM Pi AS DOUBLE
DIM Name AS STRING
```

---

## Variables and Constants

### Variable Names

- Must start with a letter (A-Z, case insensitive)
- Can contain letters, digits, and underscores
- Type suffix (%, !, #, $) is optional

```basic
X = 10
Counter% = 0
Temperature# = 98.6
UserName$ = "Alice"
My_Variable = 100
```

### Variable Declaration

Variables can be declared explicitly or used implicitly:

```basic
REM Implicit (classic BASIC style)
X = 10

REM Explicit (modern style)
DIM X AS INTEGER
X = 10
```

### Constants

```basic
CONSTANT PI = 3.14159265
CONSTANT MAX_ITEMS = 100
CONSTANT APP_NAME = "MyApp"
```

### OPTION EXPLICIT

Force all variables to be declared:

```basic
OPTION EXPLICIT
DIM X AS INTEGER  REM Required
X = 10            REM OK
Y = 20            REM Error: Y not declared
```

---

## Operators

### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| + | Addition | 5 + 3 = 8 |
| - | Subtraction | 5 - 3 = 2 |
| * | Multiplication | 5 * 3 = 15 |
| / | Division (float) | 5 / 2 = 2.5 |
| \ | Integer division | 5 \ 2 = 2 |
| ^ | Exponentiation | 2 ^ 3 = 8 |
| MOD | Modulo | 5 MOD 2 = 1 |

### Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| = | Equal | A = B |
| <> | Not equal | A <> B |
| < | Less than | A < B |
| <= | Less or equal | A <= B |
| > | Greater than | A > B |
| >= | Greater or equal | A >= B |

### Logical Operators

| Operator | Description | Behavior |
|----------|-------------|----------|
| AND | Logical AND | TRUE if both operands are TRUE |
| OR | Logical OR | TRUE if either operand is TRUE |
| NOT | Logical NOT | Inverts boolean value |
| XOR | Exclusive OR | TRUE if operands differ |
| EQV | Equivalence | TRUE if operands are same |
| IMP | Implication | FALSE only if first is TRUE and second is FALSE |

#### OPTION BITWISE vs OPTION LOGICAL

```basic
OPTION BITWISE
X = 5 AND 3     REM Bitwise: 5 AND 3 = 1

OPTION LOGICAL
X = 5 AND 3     REM Logical: TRUE AND TRUE = TRUE (non-zero = TRUE)
```

### Operator Precedence

1. Parentheses `()`
2. Exponentiation `^`
3. Unary minus `-`
4. Multiplication `*`, Division `/`, Integer Division `\`
5. Modulo `MOD`
6. Addition `+`, Subtraction `-`
7. Comparison `=`, `<>`, `<`, `<=`, `>`, `>=`
8. NOT
9. AND
10. OR, XOR
11. EQV, IMP

---

## Control Flow

### IF Statement

**Single-line IF:**
```basic
IF X > 10 THEN PRINT "X is large"
IF X > 10 THEN X = 10
```

**Multi-line IF/ENDIF:**
```basic
IF Score >= 90 THEN
  Grade$ = "A"
  PRINT "Excellent!"
ENDIF
```

**IF/ELSE:**
```basic
IF Age >= 18 THEN
  PRINT "Adult"
ELSE
  PRINT "Minor"
ENDIF
```

**IF/ELSEIF/ELSE:**
```basic
IF Score >= 90 THEN
  Grade$ = "A"
ELSEIF Score >= 80 THEN
  Grade$ = "B"
ELSEIF Score >= 70 THEN
  Grade$ = "C"
ELSE
  Grade$ = "F"
ENDIF
```

**Traditional single-line IF/THEN/ELSE:**
```basic
10 IF X > 0 THEN PRINT "Positive" ELSE PRINT "Non-positive"
```

### SELECT CASE

```basic
SELECT CASE Day
  CASE 1
    PRINT "Monday"
  CASE 2
    PRINT "Tuesday"
  CASE 3, 4, 5
    PRINT "Midweek"
  CASE 6, 7
    PRINT "Weekend"
  OTHERWISE
    PRINT "Invalid day"
ENDCASE
```

### GOTO and GOSUB

```basic
REM GOTO - Jump to a line
10 PRINT "Start"
20 GOTO 100
30 PRINT "This is skipped"
100 PRINT "End"

REM GOSUB/RETURN - Call a subroutine
10 PRINT "Main"
20 GOSUB 1000
30 PRINT "Back in main"
40 END
1000 REM Subroutine
1010 PRINT "In subroutine"
1020 RETURN
```

### ON GOTO/GOSUB

```basic
REM Branch based on expression value
10 INPUT "Choice (1-3)"; Choice
20 ON Choice GOTO 100, 200, 300
30 PRINT "Invalid choice"
40 END
100 PRINT "Option 1" : GOTO 40
200 PRINT "Option 2" : GOTO 40
300 PRINT "Option 3" : GOTO 40
```

---

## Loops

### FOR/NEXT Loop

```basic
REM Basic FOR loop
FOR I = 1 TO 10
  PRINT I
NEXT I

REM FOR loop with STEP
FOR I = 0 TO 100 STEP 10
  PRINT I
NEXT I

REM Countdown
FOR I = 10 TO 1 STEP -1
  PRINT I
NEXT I

REM Nested loops
FOR I = 1 TO 3
  FOR J = 1 TO 3
    PRINT I; ","; J
  NEXT J
NEXT I
```

### WHILE/WEND Loop

```basic
I = 1
WHILE I <= 10
  PRINT I
  I = I + 1
WEND
```

### DO/LOOP

```basic
REM DO WHILE ... LOOP (test at top)
I = 1
DO WHILE I <= 10
  PRINT I
  I = I + 1
LOOP

REM DO ... LOOP UNTIL (test at bottom)
I = 1
DO
  PRINT I
  I = I + 1
LOOP UNTIL I > 10

REM DO ... LOOP (infinite loop, use EXIT to break)
DO
  INPUT "Enter 0 to quit"; X
  IF X = 0 THEN EXIT DO
  PRINT "You entered"; X
LOOP
```

### REPEAT/UNTIL Loop

```basic
I = 1
REPEAT
  PRINT I
  I = I + 1
UNTIL I > 10
```

### EXIT Statement

```basic
REM Exit a FOR loop early
FOR I = 1 TO 100
  IF I = 50 THEN EXIT FOR
  PRINT I
NEXT I

REM Exit a WHILE loop
WHILE TRUE
  INPUT "Command"; Cmd$
  IF Cmd$ = "QUIT" THEN EXIT WHILE
  PRINT "Processing:"; Cmd$
WEND

REM Exit a function or subroutine
SUB Process()
  IF Error THEN EXIT SUB
  REM ... more code
END SUB
```

---

## Subroutines and Functions

### SUB (Subroutine)

```basic
REM Define a subroutine
SUB Greet(Name$)
  PRINT "Hello, "; Name$; "!"
END SUB

REM Call the subroutine
Greet("Alice")
CALL Greet("Bob")
```

### FUNCTION

```basic
REM Define a function
FUNCTION Add(A, B)
  Add = A + B
END FUNCTION

REM Call the function
Result = Add(5, 3)
PRINT "5 + 3 ="; Result
```

### Parameters

**Pass by value (default):**
```basic
FUNCTION Double(X)
  X = X * 2
  Double = X
END FUNCTION

A = 5
B = Double(A)
PRINT A  REM Still 5
PRINT B  REM 10
```

**Pass by reference:**
```basic
SUB Increment(BYREF X)
  X = X + 1
END SUB

A = 5
Increment(A)
PRINT A  REM Now 6
```

### Type Declarations

```basic
FUNCTION Square(X AS DOUBLE) AS DOUBLE
  Square = X * X
END FUNCTION

SUB SetName(BYREF Name$ AS STRING)
  Name$ = "John"
END SUB
```

### LOCAL and SHARED Variables

```basic
DIM GlobalVar AS INTEGER
GlobalVar = 100

SUB Test()
  LOCAL LocalVar AS INTEGER
  SHARED GlobalVar
  
  LocalVar = 10    REM Local to this SUB
  GlobalVar = 200  REM Modifies global variable
END SUB
```

### DEF FN (Single-line Functions)

```basic
DEF FN_DOUBLE(X) = X * 2
DEF FN_SQUARE(X) = X * X

PRINT FN_DOUBLE(5)  REM 10
PRINT FN_SQUARE(4)  REM 16
```


---

## Arrays

### Declaration

```basic
REM Single dimension array
DIM Numbers(100) AS INTEGER
DIM Names$(50) AS STRING

REM Multi-dimensional array
DIM Grid(10, 10) AS INTEGER
DIM Matrix#(3, 3) AS DOUBLE
```

### Array Indexing

```basic
OPTION BASE 1  REM Arrays start at index 1 (default)
DIM A(10)
A(1) = 100     REM First element
A(10) = 999    REM Last element

OPTION BASE 0  REM Arrays start at index 0
DIM B(10)
B(0) = 100     REM First element
B(10) = 999    REM Last element (11 elements total)
```

### Array Operations

**Fill entire array:**
```basic
DIM Numbers(100) AS INTEGER
Numbers() = 42  REM Fill all elements with 42
```

**Array bounds:**
```basic
DIM A(10, 5) AS INTEGER
Lower1 = LBOUND(A, 1)  REM Lower bound of dimension 1
Upper1 = UBOUND(A, 1)  REM Upper bound of dimension 1
Lower2 = LBOUND(A, 2)  REM Lower bound of dimension 2
Upper2 = UBOUND(A, 2)  REM Upper bound of dimension 2
```

### REDIM - Resize Arrays

```basic
REM Initial size
DIM A(10) AS INTEGER

REM Resize (data is lost)
REDIM A(20)

REM Resize but preserve data
REDIM PRESERVE A(30)
```

### ERASE - Clear Arrays

```basic
DIM A(100) AS INTEGER
A() = 42
ERASE A  REM Deallocate/clear array
```

### SWAP - Swap Variables

```basic
A = 10
B = 20
SWAP A, B
PRINT A  REM Now 20
PRINT B  REM Now 10
```

### INC/DEC - Increment/Decrement

```basic
Counter = 0
INC Counter     REM Counter = 1
INC Counter, 5  REM Counter = 6
DEC Counter     REM Counter = 5
DEC Counter, 2  REM Counter = 3
```

---

## User-Defined Types

### TYPE Declaration

```basic
TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

TYPE Person
    Name AS STRING
    Age AS INTEGER
    Score AS DOUBLE
END TYPE
```

### Nested Types

```basic
TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

TYPE Sprite
    Name AS STRING
    Position AS Point
    Velocity AS Point
    Active AS INTEGER
END TYPE
```

### Using Types

```basic
REM Declare variable of custom type
DIM P AS Point
P.X = 100
P.Y = 200
PRINT "Point: ("; P.X; ", "; P.Y; ")"

REM Nested member access
DIM S AS Sprite
S.Name = "Player"
S.Position.X = 50
S.Position.Y = 75
S.Velocity.X = 2.5
S.Velocity.Y = -1.5

PRINT "Sprite: "; S.Name
PRINT "Position: ("; S.Position.X; ", "; S.Position.Y; ")"
```

### Arrays of Types

```basic
TYPE Enemy
    Name AS STRING
    Health AS INTEGER
    X AS INTEGER
    Y AS INTEGER
END TYPE

DIM Enemies(10) AS Enemy

Enemies(1).Name = "Goblin"
Enemies(1).Health = 50
Enemies(1).X = 100
Enemies(1).Y = 150
```

### Multi-dimensional Type Arrays

```basic
TYPE Cell
    Value AS INTEGER
    Label AS STRING
END TYPE

DIM Grid(10, 10) AS Cell
Grid(5, 5).Value = 42
Grid(5, 5).Label = "Center"
```

---

## Input/Output

### PRINT Statement

```basic
REM Basic output
PRINT "Hello, World!"

REM Multiple items (space-separated)
PRINT "X ="; X; "Y ="; Y

REM Semicolon (no space)
PRINT "Name:"; Name$

REM Comma (tab spacing)
PRINT "A", "B", "C"

REM No newline (continue on same line)
PRINT "Loading";
PRINT ".";
PRINT ".";
PRINT "."
```

### ? (Shorthand for PRINT)

```basic
? "Hello"  REM Same as PRINT "Hello"
? X, Y     REM Same as PRINT X, Y
```

### INPUT Statement

```basic
REM Simple input
INPUT X

REM Input with prompt
INPUT "Enter your name"; Name$

REM Multiple inputs
INPUT "Enter X and Y"; X, Y

REM Input without newline after prompt
INPUT "Password"; Password$
```

### LINE INPUT

```basic
REM Read entire line (including spaces and commas)
LINE INPUT "Enter text"; Text$
```

### CLS - Clear Screen

```basic
CLS  REM Clear the terminal screen
```

### COLOR - Set Text Color

```basic
REM Set foreground color (0-15)
COLOR 14  REM Yellow text

REM Set foreground and background
COLOR 15, 1  REM White on blue
```

### LOCATE - Position Cursor

```basic
REM Position cursor at row, column
LOCATE 10, 20
PRINT "Text at row 10, column 20"
```

### AT - Position Cursor (alternate syntax)

```basic
AT 5, 10
PRINT "Row 5, Column 10"
```

---

## File I/O

### OPEN - Open File

```basic
REM Open for input
OPEN "data.txt" FOR INPUT AS #1

REM Open for output (create/overwrite)
OPEN "output.txt" FOR OUTPUT AS #2

REM Open for append
OPEN "log.txt" FOR APPEND AS #3
```

### CLOSE - Close File

```basic
CLOSE #1          REM Close specific file
CLOSE #1, #2, #3  REM Close multiple files
CLOSE             REM Close all open files
```

### PRINT# - Write to File

```basic
OPEN "output.txt" FOR OUTPUT AS #1
PRINT #1, "Hello, World!"
PRINT #1, "X ="; X
CLOSE #1
```

### INPUT# - Read from File

```basic
OPEN "data.txt" FOR INPUT AS #1
INPUT #1, Name$, Age
INPUT #1, Score
CLOSE #1
```

### LINE INPUT# - Read Line from File

```basic
OPEN "text.txt" FOR INPUT AS #1
LINE INPUT #1, Line$
PRINT Line$
CLOSE #1
```

### WRITE# - Write with Quoting

```basic
REM Writes data with quotes and commas (CSV format)
OPEN "data.csv" FOR OUTPUT AS #1
WRITE #1, "John", 30, 95.5
WRITE #1, "Jane", 25, 87.3
CLOSE #1
```

### EOF() - End of File

```basic
OPEN "data.txt" FOR INPUT AS #1
WHILE NOT EOF(1)
    LINE INPUT #1, Line$
    PRINT Line$
WEND
CLOSE #1
```

---

## Built-in Functions

### Mathematical Functions

```basic
ABS(x)          REM Absolute value
SGN(x)          REM Sign: -1, 0, or 1
INT(x)          REM Integer part (floor for positive)
FIX(x)          REM Truncate towards zero
SQR(x)          REM Square root
EXP(x)          REM e raised to x
LOG(x)          REM Natural logarithm
SIN(x)          REM Sine (radians)
COS(x)          REM Cosine (radians)
TAN(x)          REM Tangent (radians)
ATN(x)          REM Arctangent (radians)
RND(x)          REM Random number 0 to 1
```

**Random number examples:**
```basic
REM Random integer from 1 to 100
X = INT(RND(1) * 100) + 1

REM Random float from 0 to 1
Y = RND(1)
```

### String Functions

```basic
LEN(s$)         REM Length of string
LEFT$(s$, n)    REM Left n characters
RIGHT$(s$, n)   REM Right n characters
MID$(s$, start) REM Substring from start to end
MID$(s$, start, len) REM Substring of length len
LTRIM$(s$)      REM Remove leading spaces
RTRIM$(s$)      REM Remove trailing spaces
TRIM$(s$)       REM Remove leading and trailing spaces
UCASE$(s$)      REM Convert to uppercase
LCASE$(s$)      REM Convert to lowercase
STR$(x)         REM Convert number to string
VAL(s$)         REM Convert string to number
CHR$(n)         REM Character from ASCII code
ASC(s$)         REM ASCII code of first character
INSTR(s1$, s2$) REM Find s2$ in s1$ (returns position)
SPACE$(n)       REM String of n spaces
STRING$(n, c$)  REM String of n copies of c$
```

**String examples:**
```basic
Name$ = "  John Doe  "
PRINT LEN(Name$)        REM 12
PRINT TRIM$(Name$)      REM "John Doe"
PRINT UCASE$(Name$)     REM "  JOHN DOE  "

S$ = "Hello, World!"
PRINT LEFT$(S$, 5)      REM "Hello"
PRINT RIGHT$(S$, 6)     REM "World!"
PRINT MID$(S$, 8, 5)    REM "World"

X = 42
PRINT "X = " + STR$(X)  REM "X = 42"
Y = VAL("3.14")         REM 3.14
```

### Type Conversion

```basic
CINT(x)         REM Convert to integer
CSNG(x)         REM Convert to single precision
CDBL(x)         REM Convert to double precision
```

### Keyboard Input

```basic
INKEY$          REM Read key without waiting (returns "" if no key)
```

**Example:**
```basic
PRINT "Press any key to continue..."
DO
    K$ = INKEY$
LOOP UNTIL K$ <> ""
PRINT "You pressed: "; K$
```

### Time Functions

```basic
TIMER           REM Seconds since midnight (or system start)
TIME$           REM Current time as string "HH:MM:SS"
DATE$           REM Current date as string "MM-DD-YYYY"
```

### Utility Functions

```basic
IIF(condition, true_val, false_val)  REM Immediate IF (inline conditional)
```

**Example:**
```basic
Age = 20
Status$ = IIF(Age >= 18, "Adult", "Minor")
PRINT Status$  REM "Adult"
```


---

## Compiler Directives

### OPTION BASE

Set the default lower bound for arrays:

```basic
OPTION BASE 0  REM Arrays start at 0 (C-style)
OPTION BASE 1  REM Arrays start at 1 (traditional BASIC, default)
```

### OPTION EXPLICIT

Require all variables to be declared:

```basic
OPTION EXPLICIT

DIM X AS INTEGER  REM Must declare before use
X = 10            REM OK
Y = 20            REM Error: Y not declared
```

### OPTION BITWISE / OPTION LOGICAL

Control behavior of logical operators:

```basic
OPTION BITWISE
X = 5 AND 3     REM Bitwise AND: 101 AND 011 = 001 = 1
Y = 12 OR 10    REM Bitwise OR: 1100 OR 1010 = 1110 = 14

OPTION LOGICAL
X = 5 AND 3     REM Logical AND: TRUE AND TRUE = TRUE (-1)
Y = 0 OR 5      REM Logical OR: FALSE OR TRUE = TRUE (-1)
```

### OPTION UNICODE

Enable Unicode string support:

```basic
OPTION UNICODE

Name$ = "José"
City$ = "São Paulo"
Text$ = "Hello 世界"
PRINT Name$; " lives in "; City$
```

**Unicode string functions:**
```basic
OPTION UNICODE

S$ = "Hello 世界"
PRINT ULEN(S$)      REM Unicode length (7)
PRINT LEN(S$)       REM Byte length (may differ)
PRINT UCODE(S$)     REM Unicode code point of first char
PRINT UCHR$(20320)  REM Unicode character from code point
```

### OPTION ERROR

Enable line number tracking for better error messages:

```basic
OPTION ERROR

100 X = 10
110 Y = 0
120 Z = X / Y  REM Error will report line 120
```

### OPTION CANCELLABLE

Enable cancellable operations (like WAIT_MS):

```basic
OPTION CANCELLABLE

REM WAIT_MS can be interrupted by timer events
WAIT_MS 5000  REM Wait 5 seconds (can be cancelled)

OPTION CANCELLABLE OFF
WAIT_MS 5000  REM Wait 5 seconds (cannot be cancelled)
```

### OPTION FORCE_YIELD

Enable quasi-preemptive multitasking for timer handlers:

```basic
OPTION FORCE_YIELD

REM Timer handlers will force yield points
EVERY 100 MS GOSUB 1000
```

### INCLUDE

Include another BASIC file:

```basic
INCLUDE "library.bas"
INCLUDE "constants.bas"

OPTION ONCE
INCLUDE "header.bas"  REM Will only be included once even if called multiple times
```

---

## Data Statements

### DATA/READ/RESTORE

Store and read constant data:

```basic
REM Define data
DATA 10, 20, 30, 40, 50
DATA "Apple", "Banana", "Cherry"

REM Read data
READ A, B, C
PRINT A, B, C  REM 10, 20, 30

READ Fruit1$, Fruit2$
PRINT Fruit1$, Fruit2$  REM "Apple", "Banana"

REM Reset data pointer
RESTORE
READ X
PRINT X  REM 10 (reads from beginning again)
```

**RESTORE with line numbers:**
```basic
100 DATA 1, 2, 3
200 DATA 4, 5, 6

RESTORE 200
READ A, B
PRINT A, B  REM 4, 5
```

---

## Event System and Timers

FasterBASIC provides an event-driven programming model with timer support.

### AFTER - One-shot Timer

Execute code after a delay:

```basic
REM After 1000 milliseconds, jump to line 1000
AFTER 1000 MS GOTO 1000

REM After 2 seconds
AFTER 2 SECS GOSUB 2000

REM Call a subroutine
AFTER 500 MS CALL HandleDelay

REM Inline handler (modern style)
AFTER 1000 MS
    PRINT "One second elapsed!"
DONE
```

### EVERY - Repeating Timer

Execute code repeatedly:

```basic
REM Every 100 milliseconds
EVERY 100 MS GOTO 1000

REM Every 1 second
EVERY 1 SECS GOSUB 5000

REM Inline handler
EVERY 500 MS
    Counter = Counter + 1
    PRINT "Tick: "; Counter
DONE
```

### AFTERFRAMES / EVERYFRAME

Frame-based timers (useful for game loops):

```basic
REM After 60 frames
AFTERFRAMES 60 GOSUB 1000

REM Every frame (game loop)
EVERYFRAME
    UpdateGame()
    DrawScreen()
DONE
```

### TIMER STOP

Stop a specific timer:

```basic
REM Store timer ID
TimerID = AFTER 1000 MS GOTO 1000

REM Later, stop the timer
TIMER STOP TimerID
```

### ONEVENT

Trigger code on named events:

```basic
REM Define event handler
ONEVENT "collision" GOSUB 5000

REM Later, trigger the event (from code or plugin)
REM Note: Event triggering typically happens internally
```

### RUN - Main Event Loop

Start the event loop (required for timers):

```basic
REM Set up timers
EVERY 1 SECS GOSUB UpdateDisplay

REM Start event loop (program continues until quit)
RUN

REM This code runs after event loop exits
PRINT "Program ended"
END

SUB UpdateDisplay
    CLS
    PRINT "Time: "; TIMER
END SUB
```

### WAIT and WAIT_MS

```basic
REM Wait (traditional, blocks execution)
WAIT 1000  REM Wait 1000 milliseconds

REM WAIT_MS (can be cancelled with OPTION CANCELLABLE)
OPTION CANCELLABLE
WAIT_MS 5000  REM Wait 5 seconds, can be interrupted by events
```

---

## Plugin System

FasterBASIC supports extensible plugins that add new commands and functions.

### Available Plugins

#### CSV Plugin
```basic
REM CSV file operations
Handle = CSV_OPEN("data.csv", "r")
CSV_READ Handle, Row$()
CSV_CLOSE Handle
```

#### DateTime Plugin
```basic
REM Date/time operations
NOW$ = DATETIME_NOW()
Stamp = DATETIME_TIMESTAMP()
PRINT DATETIME_FORMAT(Stamp, "YYYY-MM-DD HH:MM:SS")
```

#### Environment Plugin
```basic
REM Environment variables
Home$ = ENV_GET("HOME")
ENV_SET "MY_VAR", "value"
```

#### FileOps Plugin
```basic
REM File system operations
FILEOPS_COPY "source.txt", "dest.txt"
FILEOPS_DELETE "temp.txt"
FILEOPS_MKDIR "newfolder"
Exists = FILEOPS_EXISTS("file.txt")
```

#### INI Plugin
```basic
REM INI file operations
INI_LOAD "config.ini"
Value$ = INI_GET("Section", "Key")
INI_SET "Section", "Key", "NewValue"
INI_SAVE "config.ini"
```

#### JSON Plugin
```basic
REM JSON operations
JSON_LOAD "data.json"
Name$ = JSON_GET("user.name")
Age = JSON_GET_NUM("user.age")
JSON_SET "user.name", "John"
JSON_SAVE "data.json"
```

#### Math Plugin
```basic
REM Extended math functions
Result = MATH_CEIL(3.2)    REM 4
Result = MATH_FLOOR(3.8)   REM 3
Result = MATH_ROUND(3.5)   REM 4
Result = MATH_POW(2, 8)    REM 256
Result = MATH_MIN(5, 3)    REM 3
Result = MATH_MAX(5, 3)    REM 5
```

#### Records Plugin
```basic
REM Record-based file I/O
Handle = RECOPEN("data.dat", "Person")
RECWRITE Handle, PersonRecord
RECREAD Handle, PersonRecord
RECCLOSE Handle
```

#### Template Plugin
```basic
REM Template engine
TEMPLATE_LOAD "template.txt"
TEMPLATE_SET "name", "John"
TEMPLATE_SET "age", "30"
Result$ = TEMPLATE_RENDER()
```

### Registry-based Commands

Plugins register commands and functions that can be called like built-in keywords:

```basic
REM These are dynamically registered by plugins
PLUGIN_COMMAND arg1, arg2
Result = PLUGIN_FUNCTION(x, y)
```

---

## Advanced Features

### Command-Line Arguments

Access command-line arguments in compiled programs:

```basic
REM Get argument count
ArgCount = ARGC()

REM Get specific argument
FOR I = 0 TO ArgCount - 1
    Arg$ = ARGV(I)
    PRINT "Arg "; I; ": "; Arg$
NEXT I
```

### Error Handling

```basic
REM Check for errors (implementation-specific)
ON ERROR GOTO ErrorHandler

REM Your code here
OPEN "file.txt" FOR INPUT AS #1

REM Normal exit
END

ErrorHandler:
    PRINT "An error occurred!"
    END
```

### Program Control

```basic
END         REM End program
STOP        REM Stop program (same as END)
SYSTEM      REM Exit to operating system
```

---

## Code Examples

### Example 1: Simple Calculator

```basic
REM Simple Calculator
PRINT "Simple Calculator"
PRINT "================="

INPUT "Enter first number"; A
INPUT "Enter operator (+, -, *, /)"; Op$
INPUT "Enter second number"; B

SELECT CASE Op$
    CASE "+"
        Result = A + B
    CASE "-"
        Result = A - B
    CASE "*"
        Result = A * B
    CASE "/"
        IF B = 0 THEN
            PRINT "Error: Division by zero!"
            END
        ENDIF
        Result = A / B
    OTHERWISE
        PRINT "Invalid operator!"
        END
ENDCASE

PRINT A; " "; Op$; " "; B; " = "; Result
```

### Example 2: Number Guessing Game

```basic
REM Number Guessing Game
CLS
PRINT "=== Number Guessing Game ==="
PRINT

REM Generate random number 1-100
Target = INT(RND(1) * 100) + 1
Guesses = 0

DO
    INPUT "Guess a number (1-100)"; Guess
    Guesses = Guesses + 1
    
    IF Guess < Target THEN
        PRINT "Too low!"
    ELSEIF Guess > Target THEN
        PRINT "Too high!"
    ELSE
        PRINT "Correct! You got it in "; Guesses; " guesses!"
        EXIT DO
    ENDIF
LOOP

PRINT "The number was "; Target
```

### Example 3: File Processing

```basic
REM Process a text file line by line
OPEN "input.txt" FOR INPUT AS #1
OPEN "output.txt" FOR OUTPUT AS #2

LineCount = 0
WHILE NOT EOF(1)
    LINE INPUT #1, Line$
    LineCount = LineCount + 1
    
    REM Process line (convert to uppercase)
    Line$ = UCASE$(Line$)
    
    REM Write to output
    PRINT #2, Line$
WEND

CLOSE #1
CLOSE #2

PRINT "Processed "; LineCount; " lines"
```

### Example 4: User-Defined Types

```basic
REM Student grade tracker
TYPE Student
    Name AS STRING
    ID AS INTEGER
    Grade1 AS DOUBLE
    Grade2 AS DOUBLE
    Grade3 AS DOUBLE
END TYPE

REM Create array of students
DIM Students(5) AS Student

REM Input student data
FOR I = 1 TO 3
    PRINT "Student "; I
    INPUT "  Name"; Students(I).Name
    INPUT "  ID"; Students(I).ID
    INPUT "  Grade 1"; Students(I).Grade1
    INPUT "  Grade 2"; Students(I).Grade2
    INPUT "  Grade 3"; Students(I).Grade3
    PRINT
NEXT I

REM Calculate and display averages
PRINT "=== Grade Report ==="
FOR I = 1 TO 3
    Average = (Students(I).Grade1 + Students(I).Grade2 + Students(I).Grade3) / 3
    PRINT Students(I).Name; " ("; Students(I).ID; "): "; 
    PRINT "Average = "; INT(Average * 10) / 10
NEXT I
```

### Example 5: Timer-based Counter

```basic
REM Timer-based event counter
Counter = 0
Running = 1

REM Set up repeating timer (every 1 second)
EVERY 1 SECS
    Counter = Counter + 1
    CLS
    PRINT "Counter: "; Counter
    PRINT "Press Q to quit"
DONE

REM Main loop checking for quit
DO
    K$ = INKEY$
    IF UCASE$(K$) = "Q" THEN
        Running = 0
    ENDIF
    WAIT_MS 50
LOOP UNTIL Running = 0

PRINT "Final count: "; Counter
```

---

## Appendix A: Reserved Keywords

```
ABS, AFTER, AFTERFRAMES, AND, AS, ASC, AT, ATN
BASE, BITWISE, BYREF, BYVAL
CALL, CANCELLABLE, CASE, CDBL, CHR$, CINT, CIRCLE, CIRCLEF,
CLG, CLS, COLOR, CONSTANT, COS, CSNG
DATA, DATE$, DEC, DEF, DIM, DO, DONE, DOUBLE
ELSE, ELSEIF, END, ENDCASE, ENDFUNCTION, ENDIF, ENDSUB, ENDTYPE,
EQV, ERASE, EVERY, EVERYFRAME, EXIT, EXP, EXPLICIT
FIX, FN, FOR, FORCE_YIELD, FUNCTION
GCLS, GOSUB, GOTO, GREATER_EQUAL, GREATER_THAN
HLINE
IF, IIF, IMP, IN, INC, INCLUDE, INKEY$, INPUT, INSTR, INT, INTEGER
KEY, KEYWORD_DOUBLE, KEYWORD_INTEGER, KEYWORD_LONG, KEYWORD_SINGLE, KEYWORD_STRING
LBOUND, LCASE$, LEFT$, LEN, LET, LINE, LINE_INPUT, LOCAL, LOCATE,
LOG, LOGICAL, LONG, LOOP
MID$, MOD, MS
NEXT, NOT
OFF, ON, ONCE, ONEVENT, OPEN, OPTION, OR, OTHERWISE
PLAY, PLAY_SOUND, PRESERVE, PRINT, PRINT_AT, PSET
READ, RECT, REDIM, REM, REPEAT, RESTORE, RETURN, RIGHT$, RND,
RTRIM$, RUN
SECS, SELECT, SGN, SHARED, SIN, SINGLE, SPACE$, SPREXPLODE, SPRFREE,
SPRHIDE, SPRLOAD, SPRMOVE, SPRPOS, SPRROT, SPRSCALE, SPRSHOW, SPRTINT,
SQR, STEP, STOP, STR$, STRING, STRING$, SUB, SWAP, SYSTEM
TAN, TCHAR, TCLEAR, TEXTPUT, TGRID, THEN, TIME$, TIMER, TO, TRIM$,
TSCROLL, TYPE
UBOUND, UCASE$, UNICODE, UNTIL, USING
VAL, VLINE, VSYNC
WAIT, WAIT_MS, WEND, WHEN, WHILE, WRITE
XOR
```

---

## Appendix B: Operator Precedence Table

| Priority | Operators | Associativity |
|----------|-----------|---------------|
| 1 (highest) | () | Left to right |
| 2 | ^ | Right to left |
| 3 | - (unary) | Right to left |
| 4 | *, /, \ | Left to right |
| 5 | MOD | Left to right |
| 6 | +, - | Left to right |
| 7 | =, <>, <, <=, >, >= | Left to right |
| 8 | NOT | Right to left |
| 9 | AND | Left to right |
| 10 | OR, XOR | Left to right |
| 11 (lowest) | EQV, IMP | Left to right |

---

## Appendix C: Type Suffix Reference

| Suffix | Type | Example |
|--------|------|---------|
| % | INTEGER | Count% = 10 |
| ! | SINGLE | Value! = 3.14 |
| # | DOUBLE | Pi# = 3.14159265 |
| $ | STRING | Name$ = "John" |
| (none) | Default (DOUBLE) | X = 10.5 |

---

## Appendix D: Color Codes

Standard 16-color palette (for COLOR command):

| Code | Color | Code | Color |
|------|-------|------|-------|
| 0 | Black | 8 | Dark Gray |
| 1 | Blue | 9 | Light Blue |
| 2 | Green | 10 | Light Green |
| 3 | Cyan | 11 | Light Cyan |
| 4 | Red | 12 | Light Red |
| 5 | Magenta | 13 | Light Magenta |
| 6 | Brown | 14 | Yellow |
| 7 | Light Gray | 15 | White |

---

## Appendix E: Common Idioms

### Read all lines from file
```basic
OPEN "file.txt" FOR INPUT AS #1
WHILE NOT EOF(1)
    LINE INPUT #1, Line$
    REM Process Line$
WEND
CLOSE #1
```

### Wait for keypress
```basic
DO
    K$ = INKEY$
LOOP UNTIL K$ <> ""
```

### Generate random integer in range
```basic
REM Random integer from Min to Max (inclusive)
Value = INT(RND(1) * (Max - Min + 1)) + Min
```

### Bubble sort array
```basic
FOR I = 1 TO N - 1
    FOR J = 1 TO N - I
        IF A(J) > A(J + 1) THEN
            SWAP A(J), A(J + 1)
        ENDIF
    NEXT J
NEXT I
```

### Clear input buffer
```basic
DO
    K$ = INKEY$
LOOP UNTIL K$ = ""
```

---

## Version History

**Version 1.0.0** (January 2025)
- Initial release
- Terminal-based BASIC with LuaJIT backend
- User-defined types
- Timer and event system
- Plugin architecture
- Unicode support

---

## License

Copyright © 2024-2025 FasterBASIC Project

---

*For more information, examples, and updates, visit:*  
*https://github.com/albanread/fsh*

