# ComputerCard Programming notes


## Integer types

Because integer types are so much faster than (software-emulated) floating-point on the RP2040, most variables in a ComputerCard program will be integers.

**I tend to use the signed 32-bit integer `int32_t` (defined in the `<cstdint>` header) for most variables.**

A simple `int` can be used instead, which on the RP2040 is also a signed 32-bit integer.

### Why not unsigned types?

Many people advise using unsigned types (e.g. `uint32_t`) for variables that can never be negative. These do have the advantage of double the positive range (`uint32_t` ranges between 0 and 4.2 billion, compared to ±2.1 billion for `int32_t`).
However, operations on mixed signed and unsigned types in C++ can be confusing, thanks to [integer conversion and promotion rules](http://en.cppreference.com/w/c/language/conversion.html).

These rules say that if a mathematical operation such as `a + b` is performed on two integer operands, and
- both operands are of the same size as `int` (32-bit) or larger.
- one operand is signed (e.g. `int32_t`) and the other unsigned (e.g `uint32_t),
  
then *the operand with the signed type is implicitly converted to the unsigned type*. This means that the program
```c++
int32_t  a = -10;
uint32_t b = 2;

int32_t c = a / b;
```
sets `c` not to -5, as might be expected, but to 2147483643. (`a` is implicitly converted to an `unsigned int` with value 2^32 - 10, which is then divided by 2.) 
This type of bug can be sufficiently hard to find and fix that I prefer almost all variables to be signed.

### Why not smaller types?
Many variables don't need the full range of 32-bit numbers and it's tempting to think that smaller 16- or 8-bit types might be faster. 
In fact, the opposite can be true. Compare for example a simple function to add two 32-bit numbers

```c++
#include <cstdint>
int32_t sum(int32_t a, int32_t b)
{
    return a + b;
}
```
which compiles to a two instructions (an addition, and the return)

```asm
sum(int, int):
        adds    r0, r1, r0
        bx      lr
```

The 16-bit equivalent:
```c++
#include <cstdint>

int16_t sum(int16_t a, int16_t b)
{
    return a + b;
}
```
requires an additional instruction `sxth`
```asm
sum(short, short):
        adds    r0, r1, r0
        sxth    r0, r0
        bx      lr
```
following the 32-bit addition `adds`, in order to truncate the result to 16 bits. 

Of course, the smaller types are invaluable when RAM is limited, in long arrays or audio buffers.
