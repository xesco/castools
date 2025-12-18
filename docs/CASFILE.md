# MSX Cassette Tape & CAS File Format

## Technical Specification and Reference

This document describes the MSX cassette tape protocol and CAS container format for implementation purposes. Information gathered from community knowledge, existing code implementations, and practical analysis.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [CAS File Format](#2-cas-file-format)
3. [MSX Tape Encoding](#3-msx-tape-encoding)
4. [MSX BIOS Tape Routines](#4-msx-bios-tape-routines)
5. [Examples and Analysis](#5-examples-and-analysis)
6. [Implementation Guide](#6-implementation-guide)
7. [Reference](#7-reference)
8. [Glossary](#8-glossary)

---

## 1. Introduction

In the 1980s, MSX computers used standard audio cassette tapes to store programs and data. The MSX standard defined a specific protocol for encoding digital data as audio signals using frequency shift keying (FSK), allowing any cassette recorder to store computer programs as audio tones.

To preserve these programs in the digital age, the community created the **CAS file format**—a container that stores the logical structure of MSX cassette files without the intermediate audio encoding. A CAS file captures exactly what data blocks were on the original tape, including file headers, type markers, and program data, but skips the audio representation entirely.

**The relationship between formats:**
- **MSX cassette tapes** → Audio signals (FSK encoding at 1200/2400 Hz)
- **WAV files** → Digital audio representation of tape signals
- **CAS files** → Logical data structure extracted from the tape (no audio)

When you play an MSX cassette tape, the computer decodes the audio back into data blocks. A CAS file contains those same data blocks directly, ready to be used by emulators or converted back to audio (WAV) for use with real MSX hardware.

**This document covers:**

- **CAS format** - Digital preservation container (community standard)
- **MSX tape encoding** - Physical audio protocol (official MSX standard)
- **File types** - ASCII, BASIC, and BINARY semantics
- **BIOS interface** - How MSX hardware handles tape operations
---

## 2. CAS File Format

A CAS file is a sequential container that can hold multiple files of different types (ASCII, BASIC, BINARY). Each file consists of one or more data blocks. Both files and their individual blocks are separated by CAS HEADER delimiters—an 8-byte marker that allows parsers to locate boundaries within the stream.

**CAS HEADER Structure and Alignment:**

The CAS HEADER is always 8 bytes (`1F A6 DE BA CC 13 7D 74`) and block data must be 8-byte aligned, which means CAS HEADERs are also placed at 8-byte aligned offsets (0, 8, 16, 24, ...).

Every file begins with a file header block that identifies the file type (ASCII, BASIC, or BINARY) and provides its 6-character filename. This header block is followed by one or more data blocks containing the actual file content. This is the general structure—specific details for each file type are covered in the following sections.

```
┌─────────────────────────────────────────────────┐
│                   CAS FILE                      │
├─────────────────────────────────────────────────┤
│ [CAS HEADER] ← File 1 header block              │
│ [File Header: type + name]                      │
│                                                 │
│ [CAS HEADER] ← File 1 data block 1              │
│ [Data ...]                                      │
│                                                 │
│ [CAS HEADER] ← File 1 data block 2              │
│ [Data ...]                                      │
│                                                 │
│ [CAS HEADER] ← File 2 header block              │
│ [File Header: type + name]                      │
│                                                 │
│ [CAS HEADER] ← File 2 data block 1              │
│ [Data ...]                                      │
│                                                 │
│ ... more files ...                              │
└─────────────────────────────────────────────────┘
```

The format has no directory structure, global header, or length fields. You must scan sequentially through the file, reading each HEADER to discover what follows.

### 3.1 File Header Block

Every file in a CAS image starts with a file header block. This block identifies the file type using a **type marker** and provides its name. The MSX BIOS reads this header when loading from tape to determine how to process the subsequent data blocks.

**Type Markers:**

```
| File Type | Marker Byte | Pattern (10 bytes) |
|-----------|-------------|--------------------|
| ASCII     | 0xEA        | `EA EA EA EA EA EA EA EA EA EA` |
| BINARY    | 0xD0        | `D0 D0 D0 D0 D0 D0 D0 D0 D0 D0` |
| BASIC     | 0xD3        | `D3 D3 D3 D3 D3 D3 D3 D3 D3 D3` |
```

**Filename Encoding:**

Filenames are exactly 6 bytes, space-padded. Example "HELLO":
```
48 45 4C 4C 4F 20
H  E  L  L  O  (space)
```

**Putting It All Together**

```
Offset  Bytes  Content                                    Description
------  -----  -----------------------------------------  --------------------------
0x0000  8      1F A6 DE BA CC 13 7D 74                    CAS HEADER (delimiter)
0x0008  10     EA/D0/D3 (repeated 10 times)               Type marker
0x0012  6      ASCII characters (space-padded)            Filename
------  -----  -----------------------------------------  --------------------------
Total:  24 bytes

Structure:
┌────────────────────────────────────────────────┐
│            FILE HEADER BLOCK                   │
├────────────────────────────────────────────────┤
│ [CAS HEADER: 8 bytes]                          │
│   1F A6 DE BA CC 13 7D 74                      │
│   ↑ Not part of block data                     │
├────────────────────────────────────────────────┤
│ [TYPE MARKER: 10 bytes] ← ONE OF:              │
│   EA EA EA EA EA EA EA EA EA EA  (ASCII)       │
│   D0 D0 D0 D0 D0 D0 D0 D0 D0 D0  (BINARY)      │
│   D3 D3 D3 D3 D3 D3 D3 D3 D3 D3  (BASIC)       │
│   ↑ Block data starts here                     │
├────────────────────────────────────────────────┤
│ [FILENAME: 6 bytes]                            │
│   Space-padded ASCII                           │
└────────────────────────────────────────────────┘
```

### 2.2 ASCII Files

ASCII files have a flexible structure and can span multiple blocks. Unlike BINARY and BASIC files which always use exactly two blocks, ASCII files can have as many data blocks as needed to store their content.

**Block Size:**

ASCII data blocks are typically 256 bytes each. When creating CAS files, text is divided into 256-byte chunks. The last block must contain at least one EOF marker, and is padded to 256 bytes with `0x1A` bytes. If the text length is a multiple of 256, an additional block containing only `0x1A` padding is required.

**End-of-File Detection:**

ASCII files use the byte `0x1A` (decimal 26) as an EOF marker. When the MSX reads an ASCII file, it stops reading at the first occurrence of this byte, treating it as the logical end of the file. 

**What happens after EOF:**
- Data after the `0x1A` marker within the same file is ignored (padding, garbage data)
- A CAS HEADER after the EOF marker may signal the start of a **new file** (not continuation of the current ASCII file)

**Important limitation:** Because `0x1A` serves as the EOF marker, ASCII files cannot contain this byte as part of their actual content. This is only a restriction for ASCII files—BINARY and BASIC files can include `0x1A` as regular data since they don't use an EOF marker.

**Putting it all together**

```
┌────────────────────────────────────────────────────────────────┐
│ ASCII FILE: "README" spanning 3 blocks                         │
├────────────────────────────────────────────────────────────────┤
│ BLOCK 1: File Header Block                                     │
├────────────────────────────────────────────────────────────────┤
│ Offset: 0x0000                                                 │
│ [CAS HEADER: 8 bytes]                                          │
│   1F A6 DE BA CC 13 7D 74                                      │
│ [TYPE MARKER: 10 bytes]                                        │
│   EA EA EA EA EA EA EA EA EA EA                                │
│ [FILENAME: 6 bytes]                                            │
│   52 45 41 44 4D 45                                            │
│   "README"                                                     │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 2: First Data Block                                      │
├────────────────────────────────────────────────────────────────┤
│ Offset: 0x0018                                                 │
│ [CAS HEADER: 8 bytes]                                          │
│   1F A6 DE BA CC 13 7D 74                                      │
│ [DATA: ~256 bytes of text content]                             │
│   54 68 69 73 20 69 73 20 61 20 6C 6F 6E 67 20 74 ...          │
│   "This is a long text file that spans multiple..."            │
│   "blocks. It contains documentation about the..."             │
│   "MSX cassette tape format. Since this content..."            │
│   ... (no 0x1A EOF marker in this block)                       │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 3: Second Data Block (with EOF)                          │
├────────────────────────────────────────────────────────────────┤
│ Offset: 0x0120                                                 │
│ [CAS HEADER: 8 bytes]                                          │
│   1F A6 DE BA CC 13 7D 74                                      │
│ [DATA: continuation]                                           │
│   2E 2E 2E 63 6F 6E 74 69 6E 75 65 73 20 68 65 72 ...          │
│   "...continues here with more text content."                  │
│   "This is the end of the file."                               │
│ [EOF MARKER: 1 byte]                                           │
│   1A                  ← File ends here logically               │
│ [PADDING: ignored]                                             │
│   00 00 00 00 ...     ← Everything after EOF ignored           │
└────────────────────────────────────────────────────────────────┘
```

**Key points:**
- File header block identifies type (EA) and name ("README")
- Data blocks contain actual text content
- No EOF marker in first data block → continue reading
- EOF marker (0x1A) in second data block → stop reading
- Any data after EOF is ignored (padding, garbage)
- If another CAS HEADER appears, check if it starts a NEW file


### 2.3 BASIC and BINARY Files

BASIC and BINARY files have a fixed, predictable structure. Unlike ASCII files which can span multiple blocks, these files always consist of exactly two blocks: one file header block and one data block.

**Structure:**

Both file types follow this pattern:
- **Block 1:** File header block (type marker + filename)
- **Block 2:** Data block (addresses + program bytes)

**Data Block Format:**

The data block begins with a 6-byte **data header** containing three 16-bit addresses (little-endian), followed by the program data:
- **LOAD ADDRESS** (2 bytes): Memory address where data will be loaded
- **END ADDRESS**  (2 bytes): Memory address marking the end of the data range
- **EXEC ADDRESS** (2 bytes): Memory address where execution begins (for BLOAD with ,R)

**Data Length Calculation:**

The addresses in the data header tell us how much program data to expect:
```
PROGRAM_DATA_LENGTH = END_ADDRESS - LOAD_ADDRESS
```

For example: Load=0xC000, End=0xC200 → 512 bytes (0x200) of program data

**Parsing approach:** Since BINARY/BASIC files always have exactly 2 blocks (file header + data block), and the address calculation tells you the program data length, after reading the data header and the calculated number of program data bytes, the file is complete. 

Whether there's padding after the program data depends on how the CAS file was created. The next CAS HEADER you encounter (if any) will start a NEW file (not part of the current file). You must scan forward from your current position to locate that next CAS HEADER—it could be immediately after the program data, or there might be some padding bytes in between.

**No EOF Marker:**

Since these are binary files, all byte values from `0x00` to `0xFF` are valid data. There's no special EOF marker like ASCII's `0x1A`—the addresses define exactly where the data ends.

**File ID Bytes:**

When BINARY and BASIC files are stored on disk (e.g., in MSX-DOS), they include a 1-byte file ID prefix:
- **BINARY files:** `0xFE` prefix byte (not present in CAS format)
- **BASIC files:**  `0xFF` prefix byte (not present in CAS format)

These ID bytes are **NOT** stored in CAS files—they belong to the disk file format. When extracting BINARY files from CAS, tools typically add the `0xFE` prefix to match the disk format. When adding BINARY/BASIC files to a CAS, tools automatically strip these prefix bytes if present.

**BINARY File Validation:**

When creating or reading BINARY files, implementations should validate:
1. Data block is at least 6 bytes (minimum for address header)
2. `LOAD_ADDRESS ≤ END_ADDRESS` (begin must not exceed end)
3. `END_ADDRESS - LOAD_ADDRESS` ≤ actual program data size
4. `LOAD_ADDRESS ≤ EXEC_ADDRESS ≤ END_ADDRESS` (execution address must be within range)

**BASIC File Validation:**

BASIC files must be at least 2 bytes (minimal tokenized program structure).

**Putting it all together**

```
┌────────────────────────────────────────────────────────────────┐
│ BINARY FILE: "LOADER" with 512 bytes of Z80 code               │
├────────────────────────────────────────────────────────────────┤
│ BLOCK 1: File Header Block                                     │
├────────────────────────────────────────────────────────────────┤
│ Offset: 0x0000                                                 │
│ [CAS HEADER: 8 bytes]                                          │
│   1F A6 DE BA CC 13 7D 74                                      │
│ [TYPE MARKER: 10 bytes]                                        │
│   D0 D0 D0 D0 D0 D0 D0 D0 D0 D0                                │
│   (BINARY marker)                                              │
│ [FILENAME: 6 bytes]                                            │
│   4C 4F 41 44 45 52                                            │
│   "LOADER"                                                     │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 2: Data Block                                            │
├────────────────────────────────────────────────────────────────┤
│ Offset: 0x0018                                                 │
│ [CAS HEADER: 8 bytes]                                          │
│   1F A6 DE BA CC 13 7D 74                                      │
│ [DATA HEADER: 6 bytes]                                         │
│   00 C0                   LOAD ADDRESS → 0xC000                │
│   00 C2                   END ADDRESS  → 0xC200                │
│   00 C0                   EXEC ADDRESS → 0xC000                │
│   Length = 0xC200 - 0xC000 = 512 bytes                         │
│ [PROGRAM DATA: 512 bytes]                                      │
│   21 00 C0 CD 00 00 C9 ...                                     │
│   (Z80 machine code)                                           │
└────────────────────────────────────────────────────────────────┘

```

**Key points:**
- Exactly 2 blocks, no more, no less
- Block 1: Identifies file type (D0 for BINARY) and name
- Block 2: Contains addresses and all program data
- Data length determined by END_ADDRESS - LOAD_ADDRESS
- All bytes (including 0x1A) are valid data
- After reading 2nd block, file is complete → next CAS HEADER
  starts a NEW file (if present)


```
┌────────────────────────────────────────────────────────────┐
│ BASIC FILE: "GAME  " with tokenized BASIC program          │
├────────────────────────────────────────────────────────────┤
│ BLOCK 1: File Header Block                                 │
├────────────────────────────────────────────────────────────┤
│ Offset: 0x0000                                             │
│ [CAS HEADER: 8 bytes]                                      │
│   1F A6 DE BA CC 13 7D 74                                  │
│ [TYPE MARKER: 10 bytes]                                    │
│   D3 D3 D3 D3 D3 D3 D3 D3 D3 D3                            │
│   (BASIC marker)                                           │
│ [FILENAME: 6 bytes]                                        │
│   47 41 4D 45 20 20                                        │
│   "GAME  " (space-padded)                                  │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ BLOCK 2: Data Block                                        │
├────────────────────────────────────────────────────────────┤
│ Offset: 0x0018                                             │
│ [CAS HEADER: 8 bytes]                                      │
│   1F A6 DE BA CC 13 7D 74                                  │
│ [DATA HEADER: 6 bytes]                                     │
│   00 80                   LOAD ADDRESS → 0x8000            │
│   1A 81                   END ADDRESS  → 0x811A            │
│   00 80                   EXEC ADDRESS → 0x8000            │
│   Length = 0x811A - 0x8000 = 282 bytes                     │
│                                                            │
│ [BASIC PROGRAM: 282 bytes in tokenized format]             │
│   BASIC Line 10: PRINT "Hello"                             │
│   ───────────────────────────────────────                  │
│   00 00          Next line pointer (0x0000 = no next line) │
│   0A 00          Line number (10 in little-endian)         │
│   91             PRINT keyword token                       │
│   20 22 48 65 6C 6C 6F 22    Space + "Hello" string        │
│   00             Line end marker                           │
│                                                            │
│   BASIC Line 20: END                                       │
│   ─────────────────────                                    │
│   14 00          Next line pointer (0x0014 = offset 20)    │
│   14 00          Line number (20 in little-endian)         │
│   81             END keyword token                         │
│   00             Line end marker                           │
│                                                            │
│   00 00          Program end marker                        │
└────────────────────────────────────────────────────────────┘

```

**Key points:**
- BASIC programs are stored in tokenized format, not ASCII text
- Keywords (PRINT, END, FOR, etc.) become single-byte tokens
- Tokens: 0x91 = PRINT, 0x81 = END, 0x8F = FOR, etc.
- Line numbers stored as binary values in line headers
- Each line ends with 0x00 terminator
- Program ends with 0x00 0x00 marker
- Even if program contains 0x1A byte, it's treated as data, not EOF


## 3. MSX Tape Encoding

This section describes how MSX computers actually read and write cassette tapes. While CAS files store the logical structure, real MSX hardware converts data into audio signals that can be recorded on magnetic tape. This is the official MSX standard, independent of the CAS preservation format.

### 3.1 FSK Modulation

MSX uses Frequency Shift Keying (FSK) to convert digital bits into audio tones. Each bit value is represented by a specific frequency, allowing ordinary cassette recorders to store computer data.

**Transmission:** Data is encoded as 1200 Hz or 2400 Hz tones  
**Detection:** MSX reads the tape by measuring zero-crossing timing (see section 4.2)

Supported baud rates: 1200 baud (default) or 2400 baud.

**Common WAV conversion settings** (not part of MSX standard, but work well):
- Sample rate: 43200 Hz
- Bit depth: 8-bit unsigned PCM  
- Channels: Mono

These parameters are commonly used for CAS-to-WAV conversion because 43200 Hz divides evenly into both 1200 Hz and 2400 Hz frequencies. Other settings can work as long as they accurately reproduce the FSK tones.

### 3.2 Bit Encoding

Each bit has a fixed time duration, but uses different frequencies (different numbers of wave cycles):

**1200 baud** (each bit = 833.3 µs):
- **0-bit:** 1 cycle at 1200 Hz (takes full 833.3 µs)
- **1-bit:** 2 cycles at 2400 Hz (each cycle 416.7 µs, total 833.3 µs)

**2400 baud** (each bit = 416.7 µs):  
- **0-bit:** 1 cycle at 2400 Hz (takes full 416.7 µs)
- **1-bit:** 2 cycles at 4800 Hz (each cycle 208.3 µs, total 416.7 µs)

**At 43200 Hz sample rate:**

| Baud Rate | Bit Value | Frequency   | Samples per Bit   |
|-----------|-----------|-----------  |-------------------|
| 1200      | 0-bit     | 1 × 1200 Hz | 36 samples        |
| 1200      | 1-bit     | 2 × 2400 Hz | 36 samples (2×18) |
| 2400      | 0-bit     | 1 × 2400 Hz | 18 samples        |
| 2400      | 1-bit     | 2 × 4800 Hz | 18 samples (2×9)  |

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

### 3.3 Serial Framing

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

### 3.4 Sync and Silence

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

## 4. MSX BIOS Tape Routines

| Function | Address | Purpose                      |
|----------|---------|------------------------------|
| TAPION   | #00E1 | Read header and start motor  |
| TAPIN    | #00E4 | Read one byte                |
| TAPIOF   | #00E7 | Stop reading                 |
| TAPOON   | #00EA | Write header and start motor |
| TAPOUT   | #00ED | Write one byte               |
| TAPOOF   | #00F0 | Stop writing                 |
| STMOTR   | #00F3 | Motor control                |

**Usage:** TAPION reads header, TAPIN reads bytes until error, TAPIOF stops. TAPOUT writes data with automatic serial framing and FSK encoding.

---

## 5. Examples and Analysis

### 5.1 Hex Dump Examples

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

### 5.2 Real CAS Analysis

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

## 6. Implementation Guide

### 6.1 Parsing Algorithm

To parse a CAS file, scan sequentially through the data:

1. **Search for CAS HEADER** - Look for the 8-byte pattern `1F A6 DE BA CC 13 7D 74`
2. **Read the type marker** - Next 10 bytes identify the file type (ASCII/BINARY/BASIC)
3. **Read the filename** - Next 6 bytes contain the filename (space-padded)
4. **Read data blocks:**
   - **For ASCII files:** Continue reading through subsequent blocks until you encounter `0x1A` (EOF marker)
   - **For BINARY/BASIC files:** Read exactly one more block containing addresses and program data
5. **Repeat** from step 1 to find the next file

This sequential scan is necessary because CAS files have no directory or table of contents.

### 6.2 Common Mistakes

**Treating CAS headers as data**  
The 8-byte CAS HEADER is a structural delimiter, not part of the file content. Don't include it when extracting file data.

**Mixing CAS parsing with audio encoding**  
CAS files contain logical structure only. Don't try to interpret them as audio samples or look for FSK encoding—that's only in WAV files.

**Expecting length fields**  
CAS blocks have no length headers. ASCII files end at `0x1A`, BINARY/BASIC files use address ranges to determine length.

**Treating 0x1A as structural**  
The `0x1A` byte is meaningful only for ASCII files as an EOF marker. In BINARY/BASIC files, `0x1A` is just normal data with no special meaning.

### 6.3 Practical Limits

**Filename length:** Exactly 6 bytes. Longer names are automatically truncated. Shorter names are space-padded on the right (0x20). Names with fewer than 6 printable characters are right-padded with spaces. When extracting files, trailing spaces and null bytes are stripped.

**Filename character support:** ASCII characters only. The format treats filenames as 6-byte ASCII arrays. Characters outside printable ASCII range may cause issues.

**Block alignment:** Block data must be 8-byte aligned, which means CAS HEADERs (being 8 bytes) must be placed at 8-byte aligned offsets (0, 8, 16, 24, ...). When creating CAS files, data blocks are padded with:
- **BINARY/BASIC files:** Zero bytes (0x00) for 8-byte alignment
- **ASCII files:** EOF bytes (0x1A) for 256-byte alignment
- **Custom blocks:** Zero bytes (0x00) for 8-byte alignment

**Loading time at 1200 baud:
- 1 KB file: approximately 10 seconds
- 16 KB file: approximately 2.5 minutes
- These times include sync pulses and inter-block gaps

**Memory constraints:** MSX systems typically have 8-64 KB of RAM, so most files are small by modern standards.

---

## 7. Reference

### 7.1 Tools

**CASTools** (C) - cas2wav, wav2cas, casdir utilities  
**MCP** (Rust) - https://github.com/apoloval/mcp - Create, extract, list CAS files

### 7.2 Documentation

*Compiled from official MSX documentation, BIOS behavior analysis, and real-world CAS examination.*

---

## 8. Glossary

**ASCII file**  
A text file stored in a CAS container. ASCII files can span multiple data blocks and are terminated by a `0x1A` (EOF) byte. The MSX reads ASCII files character-by-character until encountering the EOF marker. Common for BASIC program listings saved with `SAVE "CAS:filename",A`.

**BASIC file**  
A tokenized BASIC program stored in binary format. BASIC files always consist of exactly two blocks: a file header block and one data block. The data includes a 6-byte address header (load address, end address, and execution address) followed by the tokenized program where keywords are converted to single-byte tokens (e.g., `PRINT` becomes `0x91`). The address structure is identical to BINARY files. Loaded with the `LOAD "CAS:filename"` command, which recognizes the BASIC type marker (0xD3) and loads the tokenized program into BASIC's program area.

**BINARY file**  
A raw machine code program or data block stored with address information. BINARY files always consist of exactly two blocks: a file header block and one data block containing a 6-byte address header (load address, end address, and execution address) followed by the program bytes. Loaded into memory with MSX-BASIC's `BLOAD` command and optionally executed with the `,R` parameter.

**Block**  
A unit of data in a CAS file, delimited by CAS HEADERs. Each block contains either file metadata (type marker and filename) or actual file content (program data, text, etc.). ASCII files can have many data blocks, while BINARY and BASIC files always have exactly one data block following their header block.

**CAS HEADER**  
An 8-byte delimiter pattern (`1F A6 DE BA CC 13 7D 74`) that marks the beginning of every block in a CAS file. These headers allow parsers to locate block boundaries by scanning for this magic number. The CAS HEADER is not part of the actual file data—it's purely a structural marker in the CAS container format.

**File header block**  
The first block of every file in a CAS container. Contains a 10-byte type marker (identifying the file as ASCII, BASIC, or BINARY) followed by a 6-byte filename. The MSX BIOS reads this block to determine how to process subsequent data blocks.

**FSK (Frequency Shift Keying)**  
The audio encoding method used by MSX cassette tapes to convert digital bits into audio tones. Zero bits are encoded as 1200 Hz (1 cycle per bit), while one bits are encoded as 2400 Hz (2 cycles per bit). The MSX hardware detects these frequencies by measuring the time between zero-crossings of the audio waveform.

**Logical EOF**  
The semantic end of an ASCII file, marked by the first occurrence of the `0x1A` byte. When the MSX reads an ASCII file, it stops at this marker and treats any subsequent data as padding or garbage. This is distinct from structural delimiters like CAS HEADERs—`0x1A` has meaning only within ASCII file content.

**Type marker**  
A 10-byte pattern in the file header block that identifies the file type: `0xEA` repeated 10 times for ASCII files, `0xD0` repeated 10 times for BINARY files, or `0xD3` repeated 10 times for BASIC files. This tells the MSX BIOS and CAS parsing tools how to interpret the subsequent data blocks.
