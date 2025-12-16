# MSX Cassette Tape & CAS File Format

## Technical Specification and Reference

This document describes the MSX cassette tape protocol and CAS container format for implementation purposes. Information gathered from community knowledge, existing code implementations, and practical analysis.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Glossary](#2-glossary)
3. [CAS File Format](#3-cas-file-format)
4. [MSX Tape Encoding](#4-msx-tape-encoding)
5. [MSX BIOS Tape Routines](#5-msx-bios-tape-routines)
6. [Examples and Analysis](#6-examples-and-analysis)
7. [Implementation Guide](#7-implementation-guide)
8. [Reference](#8-reference)

---

## 1. Introduction

The MSX standard defines a cassette tape storage protocol using audio encoding. CAS is a community-created digital preservation format that stores the logical data structure without the audio layer.

**This document covers:**

- **CAS format** - Digital preservation container (community standard)
- **MSX tape encoding** - Physical audio protocol (official MSX standard)
- **File types** - ASCII, BASIC, and BINARY semantics
- **BIOS interface** - How MSX hardware handles tape operations
---

## 2. Glossary

**CAS HEADER** - 8-byte block delimiter in CAS files  
**Block** - Payload between two CAS headers  
**File header block** - First block of a file; defines type and name  
**ASCII file** - Text file terminated by `0x1A`  
**Binary file** - Raw memory image loaded by BLOAD  
**Logical EOF** - Semantic end of ASCII data (`0x1A`)

---

## 3. CAS File Format

A linear sequence of blocks with no directory, global header, or length fields.

### 3.1 Block Delimiter (CAS HEADER)

The CAS HEADER is a magic number that marks the start of each block in the file. It allows parsers to reliably locate block boundaries without needing length fields.

Every block begins with this 8-byte marker: `1F A6 DE BA CC 13 7D 74`

This delimiter was invented by the community for the CAS preservation format. It exists only in CAS files, never in MSX tape audio.

### 3.2 File Structure

A CAS file is organized as a sequence of blocks, each preceded by a CAS HEADER. There is no table of contents or directory—you must scan through the file sequentially to find what's inside.

```
[CAS HEADER] [BLOCK PAYLOAD]
[CAS HEADER] [BLOCK PAYLOAD]
...
```

Block length is implicit—a block ends when you encounter the next CAS HEADER or reach end-of-file. Multiple files can be concatenated sequentially, making CAS images function like tape archives.

### 3.3 File Header Block

Every file in a CAS image starts with a file header block. This block identifies the file type and provides its name. The MSX BIOS reads this header when loading from tape to determine how to process the subsequent data blocks.

**What you see in the file:**
```
[CAS HEADER: 8 bytes]  ← delimiter, not part of block data
[TYPE MARKER: 10 bytes]  ← block data starts here
[FILENAME: 6 bytes]
```

**The File Header Block contains:** TYPE MARKER (10 bytes) + FILENAME (6 bytes) = 16 bytes total

**Type markers:**

| File Type | Marker Byte | Pattern (10 bytes) |
|-----------|-------------|-------------------|
| ASCII     | 0xEA        | `EA EA EA EA EA EA EA EA EA EA` |
| BINARY    | 0xD0        | `D0 D0 D0 D0 D0 D0 D0 D0 D0 D0` |
| BASIC     | 0xD3        | `D3 D3 D3 D3 D3 D3 D3 D3 D3 D3` |

**Filename encoding:**

Filenames are exactly 6 bytes, space-padded. Example "HELLO":
```
48 45 4C 4C 4F 20
H  E  L  L  O  (space)
```

### 3.4 ASCII Files

May span multiple blocks. End logically at first `0x1A` byte (EOF marker). Data after EOF is ignored. ASCII files cannot contain `0x1A` as valid content.

**Example: Single-block ASCII file**

```
Offset  Hex                                                   ASCII
------  ----------------------------------------------------  -----
0x0000  1F A6 DE BA CC 13 7D 74                              [CAS HEADER]
0x0008  EA EA EA EA EA EA EA EA EA EA                        [ASCII marker]
0x0012  52 45 41 44 4D 45                                    README
0x0018  54 68 69 73 20 69 73 20 61 20 74 65 73 74 0D 0A    This is a test..
0x0028  46 69 6C 65 20 63 6F 6E 74 65 6E 74 2E 0D 0A 1A    File content...
0x0038  00 00 00 00 00                                       [padding/ignored]
```

Breakdown:
- `1F A6 DE BA CC 13 7D 74` - CAS HEADER
- `EA` × 10 - ASCII type marker
- `52 45 41 44 4D 45` - Filename "README"
- Text content follows
- `1A` - EOF marker (first occurrence terminates file)
- Everything after `1A` is ignored

**Example: Multi-block ASCII file**

```
[CAS HEADER]
EA EA EA EA EA EA EA EA EA EA  ← ASCII marker
4D 59 46 49 4C 45              ← filename "MYFILE"
[large text content with no 0x1A]

[CAS HEADER]
[continuation of text content]
[more text]
1A                             ← EOF marker in second block
[ignored padding]
```

### 3.5 BASIC and BINARY Files

Unlike ASCII files which can span multiple blocks, BASIC and BINARY files have a fixed structure: always exactly two blocks.

**Block 1 (File Header Block):**
- TYPE MARKER (10 bytes): either `D0` × 10 for BINARY or `D3` × 10 for BASIC
- FILENAME (6 bytes)

**Block 2 (Data Block):**
- LOAD ADDRESS (2 bytes, little-endian): where to load in memory
- END ADDRESS (2 bytes, little-endian): end of memory range
- EXEC ADDRESS (2 bytes, little-endian): where to start execution
- PROGRAM BYTES: the actual program code or data

The data length is calculated from the addresses: `LENGTH = END_ADDRESS - LOAD_ADDRESS`. Since these are binary files, all byte values from `00` to `FF` are valid—there's no special EOF marker like ASCII's `0x1A`.

**Example: BINARY file**

```
Offset  Hex                                                   ASCII
------  ----------------------------------------------------  -----
0x0000  1F A6 DE BA CC 13 7D 74                              [CAS HEADER]
0x0008  D0 D0 D0 D0 D0 D0 D0 D0 D0 D0                        [BINARY marker]
0x0012  4C 4F 41 44 45 52                                    LOADER
0x0018  1F A6 DE BA CC 13 7D 74                              [CAS HEADER]
0x0020  00 C0                                                [Load: 0xC000]
0x0022  FF C1                                                [End: 0xC1FF]
0x0024  00 C0                                                [Exec: 0xC000]
0x0026  21 00 C0 CD 00 00 C9                                 [Program bytes]
        ^^^^^^^^^^^^^^^^^^^
        Z80 code: LD HL,0xC000 / CALL 0x0000 / RET
```

Breakdown:
- Block 1 (header): CAS HEADER + `D0` × 10 + "LOADER"
- Block 2 (data): CAS HEADER + addresses + code
- Load at 0xC000, End at 0xC1FF → 512 bytes (0x200)
- Execute from 0xC000

**Example: BASIC file**

```
Offset  Hex                                                   ASCII
------  ----------------------------------------------------  -----
0x0000  1F A6 DE BA CC 13 7D 74                              [CAS HEADER]
0x0008  D3 D3 D3 D3 D3 D3 D3 D3 D3 D3                        [BASIC marker]
0x0012  47 41 4D 45 20 20                                    GAME  
0x0018  1F A6 DE BA CC 13 7D 74                              [CAS HEADER]
0x0020  00 80                                                [Load: 0x8000]
0x0022  1A 81                                                [End: 0x811A]
0x0024  00 80                                                [Exec: 0x8000]
0x0026  00 00 0A 00                                          [BASIC line header]
0x002A  91 20 22 48 65 6C 6C 6F 22                           PRINT "Hello"
0x0033  00                                                   [Line terminator]
0x0034  14 00 14 00                                          [Next line: 20]
0x0038  81                                                   [Token: END]
0x0039  00                                                   [Line terminator]
0x003A  00 00                                                [Program end]
```

Breakdown:
- Block 1: CAS HEADER + `D3` × 10 + "GAME  "
- Block 2: CAS HEADER + load/end/exec addresses + BASIC program in tokenized format
- `91` = PRINT token, `81` = END token
- BASIC uses internal format, not ASCII text

---

## 4. MSX Tape Encoding

This section describes how MSX computers actually read and write cassette tapes. While CAS files store the logical structure, real MSX hardware converts data into audio signals that can be recorded on magnetic tape. This is the official MSX standard, independent of the CAS preservation format.

### 4.1 FSK Modulation

MSX uses Frequency Shift Keying (FSK) to convert digital bits into audio tones. Each bit value is represented by a specific frequency, allowing ordinary cassette recorders to store computer data.

**Transmission:** Data is encoded as 1200 Hz or 2400 Hz tones  
**Detection:** MSX reads the tape by measuring zero-crossing timing (see section 4.2)

Supported baud rates: 1200 baud (default) or 2400 baud.

**Common WAV conversion settings** (not part of MSX standard, but work well):
- Sample rate: 43200 Hz
- Bit depth: 8-bit unsigned PCM  
- Channels: Mono

These parameters are commonly used for CAS-to-WAV conversion because 43200 Hz divides evenly into both 1200 Hz and 2400 Hz frequencies. Other settings can work as long as they accurately reproduce the FSK tones.

### 4.2 Bit Encoding

The MSX represents 0-bits and 1-bits using different audio frequencies. Each bit occupies a fixed time slot, but uses a different number of wave cycles depending on its value.

Each bit has a fixed time duration. During that time, different frequencies are used:

**1200 baud** (each bit = 833.3 µs):
- **0-bit:** 1 cycle at 1200 Hz (takes full 833.3 µs)
- **1-bit:** 2 cycles at 2400 Hz (each cycle 416.7 µs, total 833.3 µs)

**2400 baud** (each bit = 416.7 µs):  
- **0-bit:** 1 cycle at 2400 Hz (takes full 416.7 µs)
- **1-bit:** 2 cycles at 4800 Hz (each cycle 208.3 µs, total 416.7 µs)

At 43200 Hz sample rate:
- 1200 Hz cycle = 36 samples
- 2400 Hz cycle = 18 samples  
- 4800 Hz cycle = 9 samples

So at 1200 baud: 0-bit = 36 samples, 1-bit = 36 samples (2×18)

**How MSX hardware actually detects bits:**

The MSX doesn't directly measure frequencies. Instead, it measures the time between zero crossings (when the signal crosses zero amplitude) by counting CPU T-states. The process:

1. **Wait for zero crossing** - Detect when signal crosses zero (consumes up to half a cycle)
2. **Start timer** - Begin counting CPU T-states (cycles)
3. **Wait for next zero crossing** - One half-cycle later
4. **Stop timer** - End measurement (we've now consumed roughly a full cycle)
5. **Compare with thresholds** - If time is SHORT, it's a 1-bit. If LONG, it's a 0-bit

**Zero crossing intervals at 1200 baud:**
- 1200 Hz: half-cycle = 416.7 µs = 1491 T-states (LONG = 0-bit)
- 2400 Hz: half-cycle = 208.3 µs = 746 T-states (SHORT = 1-bit)

**Key point:** Although we measure the duration of one half-cycle, we consume approximately a full cycle of signal to do so (waiting for the first zero crossing, then measuring to the next one). This is why 0-bits are 1 full cycle and 1-bits are 2 full cycles.

This timing-based method is more reliable than trying to measure exact frequencies, especially with tape speed variations and analog signal degradation.

### 4.3 Serial Framing

Each byte is transmitted with start and stop bits, similar to RS-232 serial communication. This framing allows the receiving hardware to synchronize on byte boundaries.

**Frame structure:** START (0-bit) + 8 DATA bits (LSB first) + 2 STOP (1-bits) = 11 bits/byte

Timing at 1200 baud: ~9.17 ms per byte (~109 bytes/sec)

**Example: Transmitting byte 0x42 (ASCII 'B')**

```
Byte value: 0x42 = 0100 0010 binary
Transmit order (LSB first): D0=0, D1=1, D2=0, D3=0, D4=0, D5=0, D6=1, D7=0

Bit sequence at 1200 baud:
┌───────┬────┬────┬────┬────┬────┬────┬────┬────┬──────┬──────┐
│ START │ D0 │ D1 │ D2 │ D3 │ D4 │ D5 │ D6 │ D7 │STOP1 │STOP2 │
├───────┼────┼────┼────┼────┼────┼────┼────┼────┼──────┼──────┤
│   0   │ 0  │ 1  │ 0  │ 0  │ 0  │ 0  │ 1  │ 0  │  1   │  1   │
├───────┼────┼────┼────┼────┼────┼────┼────┼────┼──────┼──────┤
│1×1200 │1×  │2×  │1×  │1×  │1×  │1×  │2×  │1×  │ 2×   │ 2×   │
│  Hz   │1200│2400│1200│1200│1200│1200│2400│1200│ 2400 │ 2400 │
└───────┴────┴────┴────┴────┴────┴────┴────┴────┴──────┴──────┘

Total: 11 bits × 833.3 μs = 9.17 ms
```

**Example: Transmitting byte 0xFF (all bits set)**

```
Byte value: 0xFF = 1111 1111 binary

Bit sequence:
┌───────┬────┬────┬────┬────┬────┬────┬────┬────┬──────┬──────┐
│ START │ D0 │ D1 │ D2 │ D3 │ D4 │ D5 │ D6 │ D7 │STOP1 │STOP2 │
├───────┼────┼────┼────┼────┼────┼────┼────┼────┼──────┼──────┤
│   0   │ 1  │ 1  │ 1  │ 1  │ 1  │ 1  │ 1  │ 1  │  1   │  1   │
├───────┼────┼────┼────┼────┼────┼────┼────┼────┼──────┼──────┤
│1×1200 │2×  │2×  │2×  │2×  │2×  │2×  │2×  │2×  │ 2×   │ 2×   │
│  Hz   │2400│2400│2400│2400│2400│2400│2400│2400│ 2400 │ 2400 │
└───────┴────┴────┴────┴────┴────┴────┴────┴────┴──────┴──────┘

One 1200 Hz cycle for START, then twenty 2400 Hz cycles for the 1-bits.
```

### 4.4 Sync and Silence

Before transmitting data, MSX sends periods of silence and repetitive sync pulses. These serve multiple purposes: allowing the cassette motor to stabilize, providing timing reference for baud rate detection (by measuring zero-crossing intervals), and acting as a carrier detection signal.

**Sequence:** silence → sync pulses → data

- Long silence: 2 sec (first block)
- Short silence: 1 sec (subsequent blocks)
- Initial sync: 8000 1-bits (~6.67 sec)
- Block sync: 2000 1-bits (~1.67 sec)

**Example: Complete audio structure for a binary file**

```
File: BINARY "GAME" with 256 bytes of data

┌─────────────────────────────────────────────────────────────┐
│ BLOCK 1: File Header                                        │
├─────────────────────────────────────────────────────────────┤
│ [2 seconds of silence]                                      │
│   86,400 samples @ 43200 Hz                                 │
│   Purpose: Motor startup and stabilization                  │
├─────────────────────────────────────────────────────────────┤
│ [8000 consecutive 1-bits]                                   │
│   Each 1-bit = 2 cycles of 2400 Hz                          │
│   Total: 16,000 cycles of 2400 Hz tone                      │
│   ~6.67 seconds of high-frequency tone                      │
│   Purpose: Initial sync, baud detection                     │
├─────────────────────────────────────────────────────────────┤
│ [10 bytes: D0 D0 D0 D0 D0 D0 D0 D0 D0 D0]                   │
│   Each byte: START + 8 data + 2 STOP = 11 bits              │
│   10 bytes × 9.17 ms = ~92 ms                               │
├─────────────────────────────────────────────────────────────┤
│ [6 bytes: filename "GAME  "]                                │
│   6 bytes × 9.17 ms = ~55 ms                                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ BLOCK 2: Data Block                                         │
├─────────────────────────────────────────────────────────────┤
│ [1 second of silence]                                       │
│   43,200 samples @ 43200 Hz                                 │
│   Purpose: Inter-block gap                                  │
├─────────────────────────────────────────────────────────────┤
│ [2000 consecutive 1-bits]                                   │
│   Each 1-bit = 2 cycles of 2400 Hz                          │
│   Total: 4,000 cycles of 2400 Hz tone                       │
│   ~1.67 seconds of high-frequency tone                      │
│   Purpose: Block sync                                       │
├─────────────────────────────────────────────────────────────┤
│ [6 bytes: load/end/exec addresses]                          │
│   6 bytes × 9.17 ms = ~55 ms                                │
├─────────────────────────────────────────────────────────────┤
│ [256 bytes: program data]                                   │
│   256 bytes × 9.17 ms = ~2.35 seconds                       │
└─────────────────────────────────────────────────────────────┘

Total audio duration: ~13 seconds for this small file
```

**Example: Sync pulse pattern detail**

```
Initial sync (8000 1-bits):
┌────────────────────────────────────────────────────────────┐
│ 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ... (8000 times)           │
│ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓                            │
│ Each 1-bit = 2 cycles of 2400 Hz                           │
│ Total = 16,000 cycles of 2400 Hz                           │
│                                                            │
│ Audio waveform:                                            │
│ ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿│
│   (continuous 2400 Hz tone for 6.67 seconds)               │
│   16,000 complete wave cycles                              │
└────────────────────────────────────────────────────────────┘

This long tone serves multiple purposes:
1. Allows MSX to measure bit timing and auto-detect baud rate (by measuring zero-crossing intervals)
2. Provides stable reference for bit boundaries
3. Confirms tape is playing at correct speed (via consistent zero-crossing timing)
4. Acts as "carrier detect" signal
```

---

## 5. MSX BIOS Tape Routines

| Function | Address | Purpose |
|----------|---------|---------|
| TAPION   | #00E1 | Read header and start motor |
| TAPIN    | #00E4 | Read one byte |
| TAPIOF   | #00E7 | Stop reading |
| TAPOON   | #00EA | Write header and start motor |
| TAPOUT   | #00ED | Write one byte |
| TAPOOF   | #00F0 | Stop writing |
| STMOTR   | #00F3 | Motor control |

**Usage:** TAPION reads header, TAPIN reads bytes until error, TAPIOF stops. TAPOUT writes data with automatic serial framing and FSK encoding.

---

## 6. Examples and Analysis

### 6.1 Hex Dump Examples

#### Example 1: Binary File

```
Offset  Hex Dump                                          ASCII
------  ------------------------------------------------  ----------------
0x0000  1F A6 DE BA CC 13 7D 74 D0 D0 D0 D0 D0 D0 D0 D0  ....¼..}t........
0x0010  D0 D0 48 45 4C 4C 4F 20                          ..HELLO 
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  File header block
        1F A6 DE BA CC 13 7D 74 = CAS HEADER
        D0 D0 D0 D0 D0 D0 D0 D0 D0 D0 = BINARY marker
        48 45 4C 4C 4F 20 = "HELLO " (filename)

0x0018  1F A6 DE BA CC 13 7D 74 00 80 FF 80 00 80 01 02  ....¼..}t........
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  CAS HEADER (data block start)
        00 80 = Load address (0x8000, little-endian)
        FF 80 = End address (0x80FF, little-endian)
        00 80 = Exec address (0x8000, little-endian)
        01 02 ... = Program bytes
```

#### Example 2: ASCII File

```
Offset  Hex Dump                                          ASCII
------  ------------------------------------------------  ----------------
0x0000  1F A6 DE BA CC 13 7D 74 EA EA EA EA EA EA EA EA  ....¼..}t........
0x0010  EA EA 54 45 53 54 20 20                          ..TEST  
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  File header block
        EA EA EA EA EA EA EA EA EA EA = ASCII marker
        54 45 53 54 20 20 = "TEST  " (filename)

0x0018  1F A6 DE BA CC 13 7D 74 31 30 20 50 52 49 4E 54  ....¼..}t10 PRINT
0x0028  20 22 48 45 4C 4C 4F 22 0D 0A 32 30 20 45 4E 44   "HELLO"..20 END
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  CAS HEADER (first data block)
        31 30 20 50... = ASCII text: "10 PRINT "HELLO"\r\n20 END"

0x0038  1F A6 DE BA CC 13 7D 74 0D 0A 1A 1A 1A 1A 1A 1A  ....¼..}t........
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  CAS HEADER (second data block)
        0D 0A = \r\n (end of line)
        1A = EOF marker (first occurrence = logical end)
        1A 1A 1A... = EOF padding (ignored)
```

#### Example 3: BASIC File

```
Offset  Hex Dump                                          ASCII
------  ------------------------------------------------  ----------------
0x0000  1F A6 DE BA CC 13 7D 74 D3 D3 D3 D3 D3 D3 D3 D3  ....¼..}t........
0x0010  D3 D3 47 41 4D 45 20 20                          ..GAME  
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  File header block
        D3 D3 D3 D3 D3 D3 D3 D3 D3 D3 = BASIC marker
        47 41 4D 45 20 20 = "GAME  " (filename)

0x0018  1F A6 DE BA CC 13 7D 74 00 80 FF 82 00 80 00 00  ....¼..}t........
0x0028  0A 00 8F 20 31 30 30 30 00 14 00 A1 20 49 BC 31  ... 1000.... I.1
        ^^^^^^^^^^^^^^^^^^^^^^^^^^  CAS HEADER (data block)
        00 80 = Load address (0x8000)
        FF 82 = End address (0x82FF) 
        00 80 = Exec address (0x8000)
        00 00 0A 00 = BASIC line structure
        8F = BASIC token (FOR)
        20 31 30 30 30 = " 1000" (line number as text)
```

#### Example 4: Multi-File Archive

```
Offset  Description
------  -----------
0x0000  [HEADER] + ASCII "FILE1" header block
0x0018  [HEADER] + ASCII "FILE1" data block 1
0x0120  [HEADER] + ASCII "FILE1" data block 2 (with EOF)
0x0228  [HEADER] + BINARY "LOAD" header block
0x0240  [HEADER] + BINARY "LOAD" data block
0x2340  [HEADER] + BINARY "CODE" header block  
0x2358  [HEADER] + BINARY "CODE" data block
```

**Key observations:**
- Each CAS HEADER (`1F A6 DE BA CC 13 7D 74`) marks a new block
- Type markers immediately follow the header in file header blocks
- Addresses are stored in little-endian format
- ASCII files can span multiple blocks; EOF (`0x1A`) marks logical end
- BINARY/BASIC files always have exactly 2 blocks (header + data)

### 6.2 Real CAS Analysis

The analyzed CAS file contains four files:

1. ASCII  "Nilo"
2. ASCII  "bas2"
3. BINARY "pant"
4. BINARY "code"

### Observed Properties

- ASCII files span multiple blocks
- EOF (`0x1A`) appears inside data blocks
- Padding after EOF is present
- Binary files use exactly two blocks
- CAS headers appear inside logical ASCII flow

This confirms:
- EOF is semantic, not structural
- CAS headers are pure delimiters
- Multi-file CAS images are normal

---

## 7. Implementation Guide

### 7.1 Parsing Algorithm

To parse a CAS file, scan sequentially through the data:

1. **Search for CAS HEADER** - Look for the 8-byte pattern `1F A6 DE BA CC 13 7D 74`
2. **Read the type marker** - Next 10 bytes identify the file type (ASCII/BINARY/BASIC)
3. **Read the filename** - Next 6 bytes contain the filename (space-padded)
4. **Read data blocks:**
   - **For ASCII files:** Continue reading through subsequent blocks until you encounter `0x1A` (EOF marker)
   - **For BINARY/BASIC files:** Read exactly one more block containing addresses and program data
5. **Repeat** from step 1 to find the next file

This sequential scan is necessary because CAS files have no directory or table of contents.

### 7.2 Common Mistakes

**Treating CAS headers as data**  
The 8-byte CAS HEADER is a structural delimiter, not part of the file content. Don't include it when extracting file data.

**Mixing CAS parsing with audio encoding**  
CAS files contain logical structure only. Don't try to interpret them as audio samples or look for FSK encoding—that's only in WAV files.

**Expecting length fields**  
CAS blocks have no length headers. ASCII files end at `0x1A`, BINARY/BASIC files use address ranges to determine length.

**Treating 0x1A as structural**  
The `0x1A` byte is meaningful only for ASCII files as an EOF marker. In BINARY/BASIC files, `0x1A` is just normal data with no special meaning.

### 7.3 Practical Limits

**Filename length:** Exactly 6 bytes (shorter names are space-padded on the right)

**Block alignment:** CAS HEADER is 8 bytes, providing natural alignment for most architectures

**Loading time at 1200 baud:**
- 1 KB file: approximately 10 seconds
- 16 KB file: approximately 2.5 minutes
- These times include sync pulses and inter-block gaps

**Memory constraints:** MSX systems typically have 8-64 KB of RAM, so most files are small by modern standards.

---

## 8. Reference

### 8.1 Tools

**CASTools** (C) - cas2wav, wav2cas, casdir utilities  
**MCP** (Rust) - https://github.com/apoloval/mcp - Create, extract, list CAS files

### 8.2 Documentation

*Compiled from official MSX documentation, BIOS behavior analysis, and real-world CAS examination.*
