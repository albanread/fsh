# FasterBASIC Language Reference Manual

Version 1.0  
Copyright © 2024-2025 FasterBASIC Project
Some of these may be forward looking statements
---

## Table of Contents

1. [Introduction](#introduction)
2. [Program Structure](#program-structure)
3. [Data Types](#data-types)
4. [Variables and Constants](#variables-and-constants)
5. [Operators](#operators)
6. [Control Flow](#control-flow)
7. [Procedures and Functions](#procedures-and-functions)
8. [Arrays](#arrays)
9. [User-Defined Types](#user-defined-types)
10. [Input/Output](#inputoutput)
11. [File Operations](#file-operations)
12. [String Functions](#string-functions)
13. [Mathematical Functions](#mathematical-functions)
14. [Timer and Events](#timer-and-events)
15. [Graphics Commands](#graphics-commands)
16. [Compiler Options](#compiler-options)
17. [Plugin System](#plugin-system)
18. [Built-in Functions Reference](#built-in-functions-reference)
19. [Error Handling](#error-handling)
20. [Best Practices](#best-practices)

---

## Introduction

FasterBASIC is a modern BASIC dialect designed to be familiar to classic BASIC programmers while providing high performance through LuaJIT compilation. It supports both line-numbered and label-based programming, structured control flow, user-defined types, and extensive graphics capabilities.

### Key Features

- **Classic BASIC Compatibility**: Line numbers, GOTO, GOSUB support
- **Modern Structured Programming**: Functions, procedures, local variables
- **High Performance**: LuaJIT-powered execution
- **Rich Type System**: Integers, floats, doubles, strings, user-defined types
- **Advanced Graphics**: Sprites, primitives, text layers
- **Event System**: Timer-based events with cancellation support
- **Unicode Support**: Full UTF-8 string handling
- **Plugin Architecture**: Extensible command system

---

## Program Structure

### Line Numbers

Programs can use optional line numbers in the classic BASIC style:

```basic
10 PRINT "Hello, World!"
20 END
```

Line numbers must be integers and are typically used with GOTO/GOSUB. Modern code can omit line numbers entirely.

### Labels

Labels provide named jump targets without line numbers:

```basic
START:
PRINT "Enter a number"
INPUT N
IF N < 0 THEN GOTO START
```
Note you can use goto and gosub within reason.
There are some restrictions, you should not use goto to jump in and out of structured loops, or goto in and out of a subroutine.
It was simply not worth every degrading every programs performance to support arbitary leaps.


### Comments

```basic
REM This is a comment
' This is also a comment (single quote)
PRINT "Code" REM inline comment
```

### Program Termination

```basic
END                ' Terminate program

```

---

## Data Types

### Basic Types

| Type | Suffix | Size | Range | Description |
|------|--------|------|-------|-------------|
| `INTEGER` | `%` | 32-bit | ±2,147,483,647 | Whole numbers |
| `FLOAT` | `!` | 32-bit | ±3.4E±38 | Single precision |
| `DOUBLE` | `#` | 64-bit | ±1.7E±308 | Double precision |
| `STRING` | `$` | Variable | N/A | Text data |

### Type Suffixes

Variables can use type suffixes for implicit typing:

```basic
A% = 100        ' Integer
B! = 3.14       ' Float
C# = 1.23456789 ' Double
D$ = "Hello"    ' String
```

Note that all numeric types such as float, double, integer are implemented in the code generation using the one Lua numeric type, but types are used for internal optimizations by the compiler.

### Type Declarations

Explicit type declarations using `AS`:

```basic
DIM Count AS INTEGER
DIM Price AS DOUBLE
DIM Name AS STRING
```

---

## Variables and Constants

### Variable Declaration

```basic
' Implicit declaration (type from suffix or first use)
X = 10
Name$ = "Alice"

' Explicit declaration
DIM Age AS INTEGER
DIM Score AS DOUBLE
```

### Constants

```basic
CONSTANT PI = 3.14159265
CONSTANT MAX_PLAYERS = 4
CONSTANT APP_NAME = "My Game"
```

Constants are evaluated at compile time and cannot be changed.

### Variable Scope

- **Global**: Declared at program level, accessible everywhere
- **Local**: Declared in SUB/FUNCTION with `DIM` or `LOCAL`
- **Shared**: Global variables accessed in SUB/FUNCTION with `SHARED`

```basic
DIM GlobalVar AS INTEGER

SUB MySub()
    LOCAL LocalVar AS INTEGER
    SHARED GlobalVar
    LocalVar = 10
    GlobalVar = 20
END SUB
```

---

## Operators

### Arithmetic Operators

| Operator | Operation | Example |
|----------|-----------|---------|
| `+` | Addition | `A + B` |
| `-` | Subtraction | `A - B` |
| `*` | Multiplication | `A * B` |
| `/` | Division | `A / B` |
| `\` | Integer Division | `A \ B` |
| `^` | Exponentiation | `A ^ B` |
| `MOD` | Modulo | `A MOD B` |

### Comparison Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `=` | Equal | `A = B` |
| `<>` | Not equal | `A <> B` |
| `<` | Less than | `A < B` |
| `<=` | Less or equal | `A <= B` |
| `>` | Greater than | `A > B` |
| `>=` | Greater or equal | `A >= B` |

### Logical Operators

```basic
OPTION LOGICAL      ' Use TRUE/FALSE for logical operations
OPTION BITWISE      ' Use bitwise operations (default)

' Logical mode
IF (A > 0) AND (B > 0) THEN PRINT "Both positive"
IF (X = 0) OR (Y = 0) THEN PRINT "At least one zero"

' Bitwise mode (operates on integer bits)
Flags = Flag1 OR Flag2
Mask = Value AND &HFF
```

| Operator | Logical Mode | Bitwise Mode |
|----------|--------------|--------------|
| `AND` | Logical AND | Bitwise AND |
| `OR` | Logical OR | Bitwise OR |
| `NOT` | Logical NOT | Bitwise NOT |
| `XOR` | Logical XOR | Bitwise XOR |
| `EQV` | Equivalence | Bitwise EQV |
| `IMP` | Implication | Bitwise IMP |

### Operator Precedence

1. `()` - Parentheses
2. `^` - Exponentiation
3. `-` - Unary minus
4. `*`, `/`, `\`, `MOD` - Multiplication, division
5. `+`, `-` - Addition, subtraction
6. `=`, `<>`, `<`, `<=`, `>`, `>=` - Comparison
7. `NOT` - Logical/bitwise NOT
8. `AND` - Logical/bitwise AND
9. `OR`, `XOR` - Logical/bitwise OR, XOR
10. `EQV`, `IMP` - Equivalence, implication

---

## Control Flow

### IF Statement

```basic
' Single-line IF
IF X > 0 THEN PRINT "Positive"

' Multi-line IF
IF Score > 90 THEN
    PRINT "Grade: A"
    Bonus = 100
END IF

' IF...ELSE
IF Age >= 18 THEN
    PRINT "Adult"
ELSE
    PRINT "Minor"
END IF

' IF...ELSEIF...ELSE
IF Score >= 90 THEN
    Grade$ = "A"
ELSEIF Score >= 80 THEN
    Grade$ = "B"
ELSEIF Score >= 70 THEN
    Grade$ = "C"
ELSE
    Grade$ = "F"
END IF
```

### SELECT CASE Statement

```basic
SELECT CASE DayNum
    CASE 1
        PRINT "Monday"
    CASE 2
        PRINT "Tuesday"
    CASE 6, 7
        PRINT "Weekend"
    CASE ELSE
        PRINT "Other day"
END CASE

' With expressions
SELECT CASE Score
    CASE IS >= 90
        Grade$ = "A"
    CASE IS >= 80
        Grade$ = "B"
    CASE ELSE
        Grade$ = "F"
END CASE
```
Note the BBC BASIC case statements are
also supported if you prefer those.

### FOR Loop

```basic
' Basic FOR loop
FOR I = 1 TO 10
    PRINT I
NEXT I

' With STEP
FOR X = 0 TO 100 STEP 5
    PRINT X
NEXT X

' Countdown
FOR Count = 10 TO 1 STEP -1
    PRINT Count
NEXT Count

' Nested loops
FOR Row = 1 TO 5
    FOR Col = 1 TO 5
        PRINT Row * Col;
    NEXT Col
    PRINT
NEXT Row
```

### FOR...IN Loop

```basic
' Iterate over array
DIM Names$(5)
Names$(1) = "Alice"
Names$(2) = "Bob"
Names$(3) = "Carol"

FOR Name$ IN Names$
    PRINT Name$
NEXT Name$
```

### WHILE Loop

```basic
WHILE Condition
    ' Loop body
WEND

' Example
Count = 0
WHILE Count < 10
    PRINT Count
    Count = Count + 1
WEND
```

### DO...LOOP

```basic
' DO WHILE (condition checked at start)
DO WHILE X < 100
    X = X * 2
LOOP

' DO UNTIL (condition checked at start)
DO UNTIL Done
    PRINT "Processing..."
LOOP

' REPEAT...UNTIL (condition checked at end)
REPEAT
    INPUT "Enter password: ", Pass$
UNTIL Pass$ = "secret"
```

### GOTO and GOSUB

```basic
' GOTO - Unconditional jump
GOTO 100
100 PRINT "Jumped here"

' GOSUB - Call subroutine
GOSUB 1000
PRINT "After subroutine"
END

1000 REM Subroutine
    PRINT "In subroutine"
    RETURN
```

### ON...GOTO and ON...GOSUB

```basic
' ON...GOTO
INPUT "Select 1-3: ", Choice
ON Choice GOTO 100, 200, 300

100 PRINT "Option 1": GOTO 400
200 PRINT "Option 2": GOTO 400
300 PRINT "Option 3"
400 END

' ON...GOSUB
ON MenuChoice GOSUB HandleFile, HandleEdit, HandleView
```

### EXIT Statement

```basic
' EXIT FOR - Exit a FOR loop early
FOR I = 1 TO 1000
    IF Found THEN EXIT FOR
    ' Search logic
NEXT I

' EXIT FUNCTION - Return from function
FUNCTION FindValue(Arr(), Target)
    FOR I = 1 TO UBOUND(Arr)
        IF Arr(I) = Target THEN EXIT FUNCTION I
    NEXT I
    EXIT FUNCTION -1
END FUNCTION

' EXIT SUB - Return from subroutine
SUB ProcessData()
    IF DataEmpty THEN EXIT SUB
    ' Process logic
END SUB
```

Note that the index variable can not be
changed in a FOR NEXT loop, you can not
exit a loop by changing the index. 
Use EXIT FOR instead.

---

## Procedures and Functions

### Subroutines (SUB)

```basic
SUB Greet(Name AS STRING)
    PRINT "Hello, "; Name; "!"
END SUB

CALL Greet("Alice")
Greet("Bob")  ' CALL is optional
```

### Functions

```basic
FUNCTION Square(X AS DOUBLE) AS DOUBLE
    Square = X * X
END FUNCTION

FUNCTION Max(A AS INTEGER, B AS INTEGER) AS INTEGER
    IF A > B THEN
        Max = A
    ELSE
        Max = B
    END IF
END FUNCTION

' Using functions
Result = Square(5)
Largest = Max(10, 20)
```

### Parameter Passing

```basic
' By value (default) - copy of value passed
SUB Increment(Value AS INTEGER)
    Value = Value + 1  ' Only changes local copy
END SUB

' By reference - original variable modified
SUB IncrementRef(BYREF Value AS INTEGER)
    Value = Value + 1  ' Changes original
END SUB

DIM X AS INTEGER
X = 10
CALL Increment(X)      ' X still 10
CALL IncrementRef(X)   ' X now 11
```

### Local and Shared Variables

```basic
DIM GlobalCount AS INTEGER

SUB UpdateCount()
    ' Declare local variable
    LOCAL Temp AS INTEGER
    
    ' Access global variable
    SHARED GlobalCount
    
    Temp = 100
    GlobalCount = GlobalCount + 1
END SUB
```

### DEF FN (Single-line Functions)

```basic
' Define single-line function
DEF FN Double(X) = X * 2
DEF FN Hypotenuse(A, B) = SQR(A^2 + B^2)

' Use functions
Result = FN Double(5)
Distance = FN Hypotenuse(3, 4)
```

---

## Arrays

### Array Declaration

```basic
' 1D array (OPTION BASE 1 default - indices 1 to 10)
DIM Numbers(10) AS INTEGER

' OPTION BASE 0 - indices 0 to 9
OPTION BASE 0
DIM Values(10) AS DOUBLE

' Multi-dimensional arrays
DIM Matrix(5, 5) AS INTEGER
DIM Grid(10, 10, 10) AS DOUBLE
```

### Array Initialization

```basic
' Individual elements
Numbers(1) = 100
Numbers(2) = 200

' Fill entire array with value
Numbers() = 0

' Using expressions for dimensions
Size = 100
DIM Buffer(Size) AS INTEGER
```

### Array Operations

```basic
' REDIM - Resize array (loses data)
REDIM Numbers(20)

' REDIM PRESERVE - Resize and keep data
REDIM PRESERVE Numbers(20)

' ERASE - Deallocate array
ERASE Numbers

' SWAP - Exchange two variables/array elements
SWAP A, B
SWAP Array(1), Array(10)
```

### Array Bounds

```basic
' Get array bounds
DIM MyArray(50) AS INTEGER
Lower = LBOUND(MyArray)  ' Returns 1 (or 0 if OPTION BASE 0)
Upper = UBOUND(MyArray)  ' Returns 50

' Use in loops
FOR I = LBOUND(MyArray) TO UBOUND(MyArray)
    MyArray(I) = I * 10
NEXT I
```

### Array Arithmetic (SIMD)

```basic
' Whole array operations (optimized with SIMD)
DIM A(100) AS DOUBLE
DIM B(100) AS DOUBLE
DIM C(100) AS DOUBLE

A() = 1.0           ' Fill with constant
B() = 2.0
C() = A() + B()     ' Element-wise addition
A() = A() * 2.0     ' Scale all elements
```

---

## User-Defined Types

### Defining Types

```basic
TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

TYPE Sprite
    Name AS STRING
    Position AS Point    ' Nested type
    Active AS INTEGER
    Health AS DOUBLE
END TYPE
```

### Using Types

```basic
' Declare variable of custom type
DIM Player AS Sprite
DIM Enemy AS Sprite

' Access members
Player.Name = "Hero"
Player.Position.X = 100
Player.Position.Y = 200
Player.Health = 100.0

' Arrays of custom types
DIM Enemies(10) AS Sprite
Enemies(1).Name = "Goblin"
Enemies(1).Position.X = 50
Enemies(1).Health = 30

' 2D arrays of types
DIM Grid(10, 10) AS Point
Grid(5, 5).X = 100
Grid(5, 5).Y = 200
```

### Type Member Access in Expressions

```basic
Distance = Player.Position.X + Player.Position.Y
TotalHealth = Player.Health + Enemy.Health

IF Player.Position.X > Enemy.Position.X THEN
    PRINT "Player is to the right"
END IF
```

---

## Input/Output

### PRINT Statement

```basic
' Basic output
PRINT "Hello"
PRINT X
PRINT "Value:", X

' Multiple items (comma = tab spacing)
PRINT "Name", "Age", "Score"
PRINT Name$, Age, Score

' Semicolon = no spacing
PRINT "X="; X; " Y="; Y

' Suppress newline with trailing semicolon
PRINT "Loading";
PRINT "."

' Question mark shorthand
? "Hello World"

' Formatted output with USING
PRINT USING "###.##"; Value
PRINT USING "Name: @@@@@@@@@@"; Name$
```

### INPUT Statement

```basic
' Basic input
INPUT X
INPUT Name$

' With prompt
INPUT "Enter your name: ", Name$
INPUT "Enter age: ", Age

' Multiple values
INPUT "Enter X and Y: ", X, Y
```

### Console Output

```basic
' Output to console (separate from screen output)
CONSOLE "Debug: X =", X
CONSOLE "Warning: Invalid value"
```

### Cursor Positioning

```basic
' AT - Position cursor (row, column)
AT 10, 20
PRINT "Text at row 10, column 20"

' LOCATE - QuickBASIC style
LOCATE 5, 10
PRINT "Row 5, Column 10"

' PRINT_AT - Print at specific position
PRINT_AT 15, 25, "Positioned text"

' INPUT_AT - Input at specific position
INPUT_AT 20, 10, "Enter name: ", Name$
```

### Text Manipulation

```basic
' Put single character
TCHAR 10, 10, 65, 15  ' Put 'A' at (10,10) in color 15

' Put text with color
TEXTPUT 5, 5, "Colored text", 14

' Set text grid size
TGRID 80, 25

' Scroll text region
TSCROLL 0, 0, 80, 25, 0, -1  ' Scroll up one line

' Clear text region
TCLEAR 0, 0, 80, 25
```

### Screen Control

```basic
' Clear screen
CLS

' Set colors
COLOR 15, 1  ' White on blue background

' Wait for vertical sync
VSYNC
```

---

## File Operations

### Opening Files

```basic
' Open file for reading
OPEN "data.txt" FOR INPUT AS #1

' Open file for writing
OPEN "output.txt" FOR OUTPUT AS #2

' Open file for appending
OPEN "log.txt" FOR APPEND AS #3

' Close files
CLOSE #1
CLOSE #2
CLOSE #3

' Close all files
CLOSE
```

### Reading from Files

```basic
' Read formatted data
OPEN "scores.txt" FOR INPUT AS #1
INPUT #1, Name$, Score
CLOSE #1

' Read line by line
OPEN "data.txt" FOR INPUT AS #1
WHILE NOT EOF(1)
    LINE INPUT #1, TextLine$
    PRINT TextLine$
WEND
CLOSE #1
```

### Writing to Files

```basic
' Write formatted data
OPEN "output.txt" FOR OUTPUT AS #1
PRINT #1, "Name", "Score"
PRINT #1, Name$, Score
CLOSE #1

' Write with WRITE# (adds quotes around strings)
OPEN "data.csv" FOR OUTPUT AS #1
WRITE #1, Name$, Age, Score
CLOSE #1
```

### File Functions

```basic
' Check if at end of file
IF EOF(1) THEN PRINT "End of file reached"

' Get file position
Position = LOC(1)

' Get file size
Size = LOF(1)
```

---

## String Functions

### String Manipulation

```basic
' Length of string
Len = LEN("Hello")  ' Returns 5

' Extract substring
S$ = "Hello World"
Left$ = LEFT$(S$, 5)      ' "Hello"
Right$ = RIGHT$(S$, 5)    ' "World"
Mid$ = MID$(S$, 7, 5)     ' "World"

' Find substring position
Pos = INSTR("Hello World", "World")  ' Returns 7

' String comparison
IF STRCMP(A$, B$) = 0 THEN PRINT "Equal"

' Convert case
Upper$ = UCASE$("hello")  ' "HELLO"
Lower$ = LCASE$("HELLO")  ' "hello"

' Trim whitespace
Trimmed$ = LTRIM$(S$)     ' Left trim
Trimmed$ = RTRIM$(S$)     ' Right trim
Trimmed$ = TRIM$(S$)      ' Both ends
```

### String Conversion

```basic
' Number to string
S$ = STR$(123)      ' " 123" (with leading space)
S$ = STR$(45.67)

' String to number
X = VAL("123")      ' Returns 123
Y = VAL("45.67")    ' Returns 45.67

' Character/ASCII conversion
C$ = CHR$(65)       ' "A"
Code = ASC("A")     ' 65
```

### String Building

```basic
' Concatenation
FullName$ = FirstName$ + " " + LastName$

' Repeat character
Stars$ = STRING$(10, "*")    ' "**********"
Spaces$ = SPACE$(5)          ' "     "

' Format string
S$ = HEX$(255)               ' "FF"
S$ = OCT$(8)                 ' "10"
S$ = BIN$(5)                 ' "101"
```

### Unicode Strings

```basic
OPTION UNICODE

' Unicode string length (character count, not bytes)
Len = LEN("Hello 世界")  ' Returns 8

' Unicode substring operations
S$ = "Hello 世界"
Left$ = LEFT$(S$, 5)     ' "Hello"
Right$ = RIGHT$(S$, 2)   ' "世界"

' Unicode character at position
C$ = MID$(S$, 7, 1)      ' "世"
```

---

## Mathematical Functions

### Basic Math Functions

```basic
' Absolute value
X = ABS(-5)         ' Returns 5

' Sign
S = SGN(-10)        ' Returns -1 (or 0, 1)

' Square root
R = SQR(16)         ' Returns 4

' Power
P = POW(2, 8)       ' Returns 256

' Integer operations
I = INT(3.7)        ' Returns 3 (floor)
I = FIX(3.7)        ' Returns 3 (truncate)
I = CINT(3.7)       ' Returns 4 (round)
```

### Trigonometric Functions

```basic
' Basic trig (angles in radians)
S = SIN(X)
C = COS(X)
T = TAN(X)

' Inverse trig
A = ATN(X)          ' Arctangent
A = ASIN(X)         ' Arcsine
A = ACOS(X)         ' Arccosine

' Two-argument arctangent
Angle = ATN2(Y, X)

' Hyperbolic functions
SH = SINH(X)
CH = COSH(X)
TH = TANH(X)
```

### Logarithmic and Exponential

```basic
' Natural logarithm
L = LOG(X)          ' ln(x)

' Base-10 logarithm
L = LOG10(X)

' Exponential
E = EXP(X)          ' e^x
```

### Random Numbers

```basic
' Initialize random seed
RANDOMIZE TIMER

' Random number 0 to <1
R = RND(1)

' Random integer in range [Min, Max]
Dice = INT(RND(1) * 6) + 1          ' 1-6
Number = INT(RND(1) * 100) + 1      ' 1-100
```

### Special Math

```basic
' Minimum/maximum
Min = MIN(A, B, C)
Max = MAX(A, B, C)

' Clamp value to range
Clamped = CLAMP(Value, MinVal, MaxVal)

' Linear interpolation
Interpolated = LERP(Start, End, T)

' Degrees/radians conversion
Rads = RAD(180)     ' π
Degs = DEG(PI)      ' 180
```

---

## Timer and Events

### Timer Functions

```basic
' Get current time in seconds
T = TIMER

' Get time in milliseconds
Ms = TIMEMS

' Get frame count
Frame = FRAME
```

### One-Shot Timers

```basic
' Execute after delay
AFTER 5 SECS GOTO HandleTimeout
AFTER 1000 MS GOSUB ProcessData
AFTER 60 FRAMES CALL UpdateScreen

' Inline handler (single statement)
AFTER 2 SECS X = X + 1 DONE

' Multi-line handler
AFTER 3 SECS
    PRINT "Three seconds elapsed"
    Counter = Counter + 1
DONE
```

### Repeating Timers

```basic
' Repeat every interval
EVERY 1 SECS GOTO GameLoop
EVERY 500 MS GOSUB UpdateDisplay
EVERY 1 FRAMES CALL RenderFrame

' Inline repeating handler
EVERY 2 SECS Score = Score + 10 DONE

' Multi-line repeating handler
EVERY 1 SECS
    PRINT "Tick: "; TIMER
    Updates = Updates + 1
DONE
```

### Frame-Based Timers

```basic
' One-shot frame timer
AFTERFRAMES 60 GOTO NextLevel

' Repeating frame timer
EVERYFRAME CALL GameUpdate
```

### Timer Control

```basic
' Stop a specific timer
TIMER STOP TimerID

' Stop all timers
TIMER STOP

' Cancellable operations
OPTION CANCELLABLE

WAIT_MS 5000  ' Wait 5 seconds (can be cancelled by timers/events)

' Main event loop
RUN  ' Run until program ends
```

### Event Handlers

```basic
' Register event handler
ONEVENT "collision" GOSUB HandleCollision
ONEVENT "keypress" GOTO ProcessKey
ONEVENT "timeout" CALL OnTimeout

' Main event loop
RUN

HandleCollision:
    PRINT "Collision detected!"
    RETURN
```

---

## Graphics Commands

### Video Modes

```basic
' Set video mode
VMODE 2         ' XRES mode (320x240)
VMODE 6         ' High resolution mode
```

### Drawing Primitives

```basic
' Plot pixel
PSET X, Y, Color

' Draw line
LINE X1, Y1, X2, Y2, Color

' Draw rectangle
RECT X, Y, Width, Height, Color

' Draw filled rectangle
RECTF X, Y, Width, Height, Color

' Draw circle
CIRCLE X, Y, Radius, Color

' Draw filled circle
CIRCLEF X, Y, Radius, Color

' Horizontal line (fast)
HLINE X, Y, Length, Color

' Vertical line (fast)
VLINE X, Y, Length, Color
```

### Graphics Control

```basic
' Clear graphics
CLG
GCLS  ' Backwards compatible

' Set drawing color
COLOR ForeColor, BackColor
```

### Sprite System

```basic
' Load sprite
SPRLOAD 1, "player.png"
SPRLOAD 2, "enemy.png"

' Show/hide sprite
SPRSHOW 1
SPRHIDE 2

' Position sprite
SPRMOVE 1, X, Y

' Advanced positioning with transform
SPRPOS 1, X, Y, ScaleX, ScaleY, Rotation, AnchorX, AnchorY

' Tint sprite
SPRTINT 1, Red, Green, Blue, Alpha

' Scale sprite
SPRSCALE 1, ScaleX, ScaleY

' Rotate sprite
SPRROT 1, Angle

' Animated sprite effect
SPREXPLODE 1, Duration

' Free sprite resources
SPRFREE 1
```

### Collision Detection

```basic
' Enable collision system
VCOLLISION_ENABLE

' Enable specific detection
VCOLLISION_ENABLE_SPRITE_DETECTION 1
VCOLLISION_ENABLE_REGION_DETECTION 1

' Add collision region
RegionID = VCOLLISION_REGION_ADD(X, Y, Width, Height, Active, UserData)

' Check sprite-sprite collision
IF VCOLLISION_SPRITE(Sprite1, Sprite2) THEN
    PRINT "Sprites collided!"
END IF

' Handle region events
ONEVENT "region_enter" GOSUB OnRegionEnter
ONEVENT "region_exit" GOSUB OnRegionExit
```

### Palette Operations

```basic
' Set palette color
PALETTE Index, Red, Green, Blue

' Global palette for XRES mode
XRES_PALETTE_GLOBAL Index, R, G, B
```

### Procedural Patterns

```basic
' Generate procedural pattern
XRES_GENERATE_PATTERN Layer, X, Y, Width, Height, PatternParams

' Flood fill
XRES_FLOOD_FILL Layer, X, Y, Color
WRES_FLOOD_FILL Layer, X, Y, Color
```

---

## Compiler Options

### Setting Options

Options control compiler behavior and must appear before code that uses the feature.

```basic
' Array base index (0 or 1)
OPTION BASE 0      ' Arrays start at 0
OPTION BASE 1      ' Arrays start at 1 (default)

' Logical vs. Bitwise operators
OPTION LOGICAL     ' AND/OR/NOT are logical operators
OPTION BITWISE     ' AND/OR/NOT are bitwise operators (default)

' Explicit variable declaration
OPTION EXPLICIT    ' All variables must be declared with DIM

' Unicode string support
OPTION UNICODE     ' Enable UTF-8 string handling

' Error line tracking
OPTION ERROR       ' Track line numbers for error reporting

' File inclusion
OPTION ONCE        ' Include file only once (for headers)

' Loop cancellation
OPTION CANCELLABLE ' Allow timer events to cancel WAIT/loops
OPTION CANCELLABLE OFF ' Disable cancellation

' Quasi-preemptive handlers
OPTION FORCE_YIELD ' Force yield points in tight loops
```

### Include Files

```basic
' Include external file
INCLUDE "library.bas"

' Include with once guard
OPTION ONCE
INCLUDE "header.bas"
```

---

## Plugin System

### Registry Commands

FasterBASIC supports extensible commands through a plugin registry:

```basic
' Commands are registered by plugins
' Example plugin commands:

' CSV Plugin
CSV_OPEN "data.csv"
CSV_READ Row$
CSV_CLOSE

' DateTime Plugin
DateStr$ = DATE$()
TimeStr$ = TIME$()
Timestamp = UNIXTIME()

' JSON Plugin
JSON_PARSE Text$, Object
Value$ = JSON_GET(Object, "key")

' Math Plugin (extended functions)
Result = ATAN2(Y, X)
Result = HYPOT(A, B)

' Environment Plugin
Path$ = ENV$("PATH")
ENV_SET "MY_VAR", "value"

' File Operations Plugin
SIZE = FILESIZE("data.bin")
EXISTS = FILEEXISTS("config.txt")
FILECOPY "src.txt", "dest.txt"
FILEMOVE "old.txt", "new.txt"

' INI File Plugin
INI_OPEN "config.ini"
Value$ = INI_GET("Section", "Key")
INI_SET "Section", "Key", "Value"
INI_CLOSE

' Records Plugin (structured data storage)
Handle = RECOPEN("data.dat", "TypeName")
RECWRITE Handle, RecordVar
RECREAD Handle, RecordVar
RECCLOSE Handle

' Template Engine Plugin
TPL_LOAD "template.html"
TPL_SET "name", Value$
Result$ = TPL_RENDER()
```

### Plugin Registration

Plugins are loaded automatically from the `plugins/` directory. They can register:

- Commands (statements)
- Functions (expressions)
- Event handlers
- Custom data types

---

## Built-in Functions Reference

### Numeric Functions

```basic
ABS(x)              ' Absolute value
SGN(x)              ' Sign: -1, 0, or 1
INT(x)              ' Integer part (floor)
FIX(x)              ' Truncate towards zero
CINT(x)             ' Round to nearest integer
SQR(x)              ' Square root
POW(x, y)           ' x raised to power y
EXP(x)              ' e raise
