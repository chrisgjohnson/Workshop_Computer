# ComputerCard programming notes


# Assembly language
The RP2040 has two ARM Cortex M0+ cores, implementing the ARMv6-M architecture. The important information for understanding these are in §2.4.3 of RP2040 datasheet and in ARM's online [instruction set summary and timings](https://developer.arm.com/documentation/ddi0484/c/Programmers-Model/Instruction-set-summary) and [details of each instruction](https://developer.arm.com/documentation/ddi0419/c/Application-Level-Architecture/The-ARMv6-M-Instruction-Set), in the M0+ and ARMv6-M manuals respectively.

Alongside the usual binary formats (UF2, BIN etc) the RPi Pico SDK compiles also outputs a disassembly file `.dis` in the `build/` directory, containing the full assembly language output of the program. 
This output is very helpful for understanding what the RP2040 is actually executing. In particular, it's easy to see which code is on the flash program card (address `10xxxxxx`) and which is in RAM (address `20xxxxxx`), and what calls into the Pico SDK library (including 'hidden' ones such as the sofware floating point implementation) are actually doing.

The online tool (Compiler Explorer)[https://godbolt.org/] is also helpful for exploring how particular C/C++ constructs compile to assembly language. To generate RP2040 code, the settings to use are the compiler `ARM GCC xxx (unknown eabi)` with flags `-mthumb -mcpu=cortex-m0 -O2`. Compiler Explorer doesn't know about the RPi Pico SDK, so is best for looking at user (i.e. your) code.

# Memory
Though the memory map is outlined in the datasheet (§2.2), this (blog post)[https://petewarden.com/2024/01/16/understanding-the-raspberry-pi-picos-memory-layout/] is more readable, and has a useful warning about the stack when using two cores.

I haven't figured out how the heap allocator actually works, but my experience has been that heap allocation (`malloc`/`free` or `new`/`delete`) is not as optimised as one might like on a memory-constrained platform. I would usually recommend structing programs to avoid repeated `malloc`/`free`, or using a custom allocator if this is necessary.

# Integer types

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
sets `c` not to -5, as might be expected, but to 2147483643. (`a` is implicitly converted to an `unsigned int` with value $2^{32} - 10$, which is then divided by 2.) 
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


# (Pseudo-) random numbers

### Generation
The pseudo-random numbers required for musical purposes do not usually need to be of particularly high statistical quality, but the generator does need to be fast, so that it imposes minimal computational load even if random numbers are generated every audio sample.

I have favoured writing custom random number generators rather than using the C library function `rand()` (from `<stdlib.h>`) or the C++11 generators in `<random>`. This is firstly because the library generators are typically higher quality (and therefore slower) than is necessary, and secondly because details such as the worst-case (as opposed to average) execution time are not clear.

Two fast algorithms on the RP2040 are a 32-bit linear congruential generator:
```c++
uint32_t lcg_u32()
{
	static uint32_t lcg_seed = 1;
	lcg_seed = 1664525 * lcg_seed + 1013904223;
	return lcg_seed;
}
```
and 32-bit xorshift:
```c++
uint32_t xorshift32()
{
	static uint32_t x = 556381194;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return x;
}
```
These are of similar speed, about 95ns per call (at 200MHz clock). By comparison, the C library `rand()` and C++ library `std::minstd_rand` both of take around 300ns, and the time available for one call of `ProcessSample()` is around 20,000ns.

I have used variations of `lcg_u32` in Utility Pair and Reverb+, but this algorithm suffers from the problem that the low bits are not very random.
Specifically, the least-significant bit of successive `lgc_u32` calls alternates between `0` and `1`, and the maximum period of each successive bit is only twice that than the last. This can be seen in the binary output of the first 16 calls to `lcg_u32`, where the last, penultimate and antepenultimate bits repeat the patterns `01`, `0110`, and `10110100` (paradiddle!), respectively:
```
00111100100010000101100101101100
01011110100010001000010111011011
10000001000101100000000101111110
10110100011100110011101011000101
00001100111100000110110101100000
01011110100110001100000100111111
11000110010101101101110110010010
10001110011000100101111111001001
00000100001110001110011010010100
10100011101001011010000011100011
01000000000111011001000011100110
01101100001000001111001100001101
10010111001101110111100100001000
11010110010000010100100011000111
00111100001011011110111101111010
11111011000110001011100010010001
```
More significant bits have much better random properties. To generate a 12-bit random number, using the most significant 12 bits (with `lcg_u32() >> 20`) is therefore much more random than using the least significant bits (with `lcg_u32 ( )&0xFFF`).

The `xorshift32` function doesn't suffer from a comparable problem, though as a very simple generator it still fails more sophisticated statistical tests.

`lcg_u32()` is a *full-period* generator, meaning that successive calls will generate all $2^{32}$ values that can be stored in a 32-bit integer, after which the sequence of values repeats. A quirk of `xorshift32` is that it will never generate the value zero, and so has period $2^{32}-1$. In either case, the period (over 4 billion) is likely to be sufficient in a musical context.

Either generator can generate signed random numbers simply by changing the return type to `int32_t`.

### Seeding
In the examples above, the initial *seed* for the random number generation is fixed and arbitrarily chosen.
On the Workshop System, it may often make sense to seed the generator either from 32 bits of the unique flash card identifier (`UniqueCardID()` in ComputerCard), or from a sources of true random 'entropy'. (In either case, the `static` variable definition in the functions above must be moved outside the generation function.)

The Pico SDK provides random number generation functions through the [`pico_rand`](https://www.raspberrypi.com/documentation/pico-sdk/high_level.html#group_pico_rand) header. These are too slow for most applications in ComputerCard, but are seeded with random entropy sourced from various parts of the RP2040 hardware. A call to one of the functions in this header (e.g. `get_rand_32`) is a good source of seed for one of the faster pseudo-random generators above, but should be called outside of the `ProcessSample` function as generating the random entropy takes ~1ms, much longer than the ~20us allowed by `ProcessSample`.

# Speed of mathematical operations

It's difficult to benchmark operations in isolation because their speed depends on the context of the surrounding code. The timings here, all at 200MHz, are therefore very approximate.

On the RP2040, 32-bit `+`, `-`, `*`, bitshifts (`<<`, `>>`) and bitwise operators (`|`, `&`, `^`) are fast single-cycle instructions (though loading operands may well take several more cycles). A single cycle at 200MHz is 5ns.

All other operations are emulated by functions, which have some function call overhead.
- 32-bit division and modulus `/` and `%` are handled by an 8-cycle hardware divider in the RP2040, and take ~120ns. In principle, functions in the Pico SDK `hardware_divider` header, with reduced function call overhead might speed this up.
- For 64-bit integers, `+` and `-` take ~50ns, `*` takes ~175ns and `/` and `%` take ~250ns.
- Single-precision floating-point operations (`+`, `-`, `*`, `/`) are of the order 400ns. 

The executive summary is: wherever performance really matters, stick with 32-bit integer `+`, `-`, `*`, bitshifts and bitwise operators, as far as possible.


### Operations vs loads/stores
As noted above, the time taken to load operands into registers may itself take several cycles. The two random number generation algorithms above make for an interesting comparison.

The linear congruential generator `lcg_u32` has only two integer operations, `*` and `+`, each done in a single cycle with `muls` and `add` instructions respectively. But the expression requires two integer constants, each requiring a two-cycle load instruction `ldr`. The result is a 14-cycle execution, of which only 2 cycles are the underlying mathematical operations.
```asm
100002d4 <_Z7lcg_u32v>:
100002d4:	4b04      	ldr	r3, [pc, #16]	@ (100002e8 <_Z7lcg_u32v+0x14>)
100002d6:	4805      	ldr	r0, [pc, #20]	@ (100002ec <_Z7lcg_u32v+0x18>)
100002d8:	681a      	ldr	r2, [r3, #0]
100002da:	4350      	muls	r0, r2
100002dc:	4a04      	ldr	r2, [pc, #16]	@ (100002f0 <_Z7lcg_u32v+0x1c>)
100002de:	4694      	mov	ip, r2
100002e0:	4460      	add	r0, ip
100002e2:	6018      	str	r0, [r3, #0]
100002e4:	4770      	bx	lr
100002e6:	46c0      	nop			@ (mov r8, r8)
100002e8:	20000f5c 	.word	0x20000f5c
100002ec:	0019660d 	.word	0x0019660d
100002f0:	3c6ef35f 	.word	0x3c6ef35f
```
By contrast, `xorshift32` has six operations - three XOR and three bitshifts, executed with single-cycle `eors` and `lsls`, but requires only the load and store of the `static` state variable (which is also required by `lcg_u32`). This executes in 12 cycles, marginally quicker than `lcg_u32`.
```asm
100002f4 <_Z10xorshift32v>:
100002f4:	4904      	ldr	r1, [pc, #16]	@ (10000308 <_Z10xorshift32v+0x14>)
100002f6:	680b      	ldr	r3, [r1, #0]
100002f8:	035a      	lsls	r2, r3, #13
100002fa:	405a      	eors	r2, r3
100002fc:	0c53      	lsrs	r3, r2, #17
100002fe:	4053      	eors	r3, r2
10000300:	0158      	lsls	r0, r3, #5
10000302:	4058      	eors	r0, r3
10000304:	6008      	str	r0, [r1, #0]
10000306:	4770      	bx	lr
10000308:	20000f58 	.word	0x20000f58
```
Such timings will change if the function calls are inlined. In particular, successive calls to the linear congruential generator `lcg_u32` can be made much more quickly if the function is inlined and the constants are retained in registers between successive calls. That could be useful for, for example, rapidly filling an audio buffer with white noise. 

### Fast multiplication
The `muls` instruction multiplies two (32-bit) registers and returns the *least significant* 32 bits of the result. As a result, from a single `muls`, we can only get the most significant bits of a multiplication by ensuring that the result does is no greater than can be stored than 32 bits (or 31 bits, for signed integers). This allows combinations such as 16×16-bit multiply (unsigned), or 15×15-bit (signed), or some unequal 8×24-bit (unsigned). This is usually acceptable on the workshop system, where most audio and CV values are 12-bit.

Within C/C++, we can extract the *most* significant 32 bits of a 32×32-bit multiply by putting the two values to be multiplied `int64_t` variables, and then selecting the upper 32 bits of the result. This requires a full 64-bit multiply and is fairly slow. We can do a bit better with code like:
```c++
// Approximate (a*b)>>31
int32_t mul32x32(uint32_t a, int32_t b)
{
	int32_t al = a & 0xFFFF;
	int32_t bl = b & 0xFFFF;
	int32_t ah = a >> 16;
	int32_t bh = b >> 16;
	int32_t ahbl = ah*bl;
	int32_t albh = al*bh;
	int32_t ahbh = ah*bh;

	return (ahbh << 1) + (albh >> 15) + (ahbl >> 15);
}
```
which constructs the desired most significant bits of the multiplication. This implementation is approximate in that the carry bit resulting from `al*bl` is ignored, so may have an error in the least significant bit of those returned.

#### The interpolator
The RP2040 contains a two specialised interpolator units per CPU core, detailed in the RP2040 datasheet section 2.3.1.6, which perform a series of mathematical operations in one clock cycle. When in *blend mode*, these have the capability of performing an 8×32-bit multiply and returning the most significant 32-bits of the result.


# Running code from RAM

By default, the RP2040 runs program code directly from the flash chip on the program card (so-called 'execute in-place', or XIP), but can also run code stored in its internal RAM. Reading from the flash chip is more than an order of magnitude slower than reading from RAM, and this has a corresponding impact on the speed at which code stored in flash can runs.

The situation is mitigated by a caching: data read from the flash card is stored in 16kB of dedicated RAM, which allows frequently-used code to be accessed at RAM speeds. For many small ComputerCard programs, this cache is sufficient to store all or nearly all the code executed every sample, and execution speed is nearly that of a program stored entirely in RAM. For larger programs, which don't fit into the cache, or those with particularly tight timing requirements, it is necessary to explicitly copy the code to RAM and run it from there.


The easiest option is to copy *all* program code to RAM before it is executed. In the RPi Pico SDK, this can be done by addding
`pico_set_binary_type(${PROJECT_NAME} copy_to_ram)`
to the `CMakeLists.txt` file.
Obviously this is only possible if the total code size, plus any RAM used to store data during the course of the program, is smaller than the ~256kB of available RAM. The linker will produce an error if this is not the case.

<details>
	<summary>Determining the size of the program</summary>
	The amount of data written to the flash program card is the size of the `.bin` file in the CMake `build/` directory, or about half the size of the `.uf2`. This almost certainly includes some constant data as well as program code. Even if program code is copied to RAM, the constant data (stored in the `rodata` section) is not. This constant data can be a significant part of the program size if, for example, there are large precalculated lookup tables in the code.
</details>
	
Another option, which is used in ComputerCard itself, is to decorate functions and class methods that should be stored in RAM with ` __time_critical_func`, e.g.
```c++
int32_t MyFunction(int32_v a) {...}
```
becomes
```c++
int32_t __time_critical_func(MyFunction)(int32_v a) {...}
```

There is a small overhead in jumping between code executed in RAM and in flash, so my approach has been to liberally apply `__time_critical_func` to all frequently-run code. For example, in ComputerCard, `ProcessSample` and all the methods likely to be called within it (`KnobVal`, `CVOut1`, etc.) are all specified as running from RAM. This process may require going into any and all library code called from `ProcessSample` to apply `__time_critical_func` to methods there.

N.B. `__time_critical_func` currently does the same as `__not_in_flash_func` (which is used in ComputerCard), but the SDK indicates that `__time_critical_func` is preferred and may have further optimisations in future.

### Checking where code will run from
The `.dis` disassembly files generated in the CMake `build/` directory show each assembly language instruction in the program and the address in memory at which it is stored. Addresses are eight-digit hexadecimal values. Those starting `1xxxxxxx` correspond to data stored in flash, and those starting `2xxxxxxx` correspond to data stored in RAM

### Warming up the cache
Where cache or RAM size limits force some code to be run directly from flash, the first few passes through a loop can be slower than subsequent passes, where the XIP cache is better filled. In the Radio Music source code, certain CPU-intensive parts of the algorithm are turned off for the first few samples, so that the initial, slower, code execution does not cause a buffer underflow.

# Writing to flash



# Euclidean rhythms and sigma-delta modulation

Curiously, the algorithm used to create Euclidean rhythms in Utility Pair is exactly the same as the algorithm used to generate precise 19-bit CV outputs (ComputerCard `CVOutPrecise` functions) from the only 11 bits of PWM resolution.


