# MSX Cassette Tape & CAS File Format

## Technical Specification and Reference

This document describes the MSX cassette tape protocol and CAS container format for implementation purposes. Information gathered from community knowledge, existing code implementations, and practical analysis.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [CAS File Format](#2-cas-file-format)
   - 2.1 [File Header Block](#21-file-header-block)
   - 2.2 [ASCII Files](#22-ascii-files)
   - 2.3 [BASIC and BINARY Files](#23-basic-and-binary-files)
3. [MSX Tape Encoding](#3-msx-tape-encoding)
   - 3.1 [FSK Modulation](#31-fsk-modulation)
   - 3.2 [Bit Encoding](#32-bit-encoding)
   - 3.3 [Serial Framing](#33-serial-framing)
   - 3.4 [Sync and Silence](#34-sync-and-silence)
   - 3.5 [Mapping CAS Structure to Audio Encoding](#35-mapping-cas-structure-to-audio-encoding)
4. [Implementation Guide](#4-implementation-guide)
   - 4.1 [CAS to WAV Conversion](#41-cas-to-wav-conversion)
   - 4.2 [WAV to CAS Conversion (wav2cas algorithm)](#42-wav-to-cas-conversion-wav2cas-algorithm)
   - 4.3 [Practical Limits](#43-practical-limits)
5. [Glossary](#5-glossary)
6. [Reference](#6-reference)

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

---

## 2. CAS File Format: general structure

A CAS file is a sequential container that can hold multiple files of different types (`ASCII`, `BASIC`, `BINARY`). Each file consists of one or more data blocks. Both files and their individual blocks are separated by `CAS HEADER` delimiters—an 8-byte marker that allows parsers to locate boundaries within the stream.

**CAS HEADER Structure and Alignment:**

The `CAS HEADER` is always 8 bytes (`1F A6 DE BA CC 13 7D 74`) and block data must be 8-byte aligned, which means `CAS HEADER`s are also placed at 8-byte aligned offsets (0, 8, 16, 24, ...).

Every file begins with a file header block that identifies the file type and provides its 6-character filename. This header block is followed by one or more data blocks containing the actual file content.

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

### 2.1 File Header Block

This block identifies the file type using a **type marker** and provides its name. The MSX BIOS reads this header when loading from tape to determine how to process the subsequent data blocks.

```
| File Type | Marker Byte | Pattern (10 bytes)            |
|-----------|-------------|-------------------------------|
| ASCII     | 0xEA        | EA EA EA EA EA EA EA EA EA EA |
| BINARY    | 0xD0        | D0 D0 D0 D0 D0 D0 D0 D0 D0 D0 |
| BASIC     | 0xD3        | D3 D3 D3 D3 D3 D3 D3 D3 D3 D3 |
```

**Filename Encoding:**

Filenames are exactly 6 bytes using ASCII character encoding, space-padded (0x20) to the right if shorter than 6 characters. Example "HELLO":
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
Total:  24 bytes (8-byte delimiter + 16 bytes of block data)

┌────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter              │
│   1F A6 DE BA CC 13 7D 74                      │
└────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────┐
│         FILE HEADER BLOCK (16 bytes)           │
├────────────────────────────────────────────────┤
│ [TYPE MARKER: 10 bytes] ← ONE OF:              │
│   EA EA EA EA EA EA EA EA EA EA  (ASCII)       │
│   D0 D0 D0 D0 D0 D0 D0 D0 D0 D0  (BINARY)      │
│   D3 D3 D3 D3 D3 D3 D3 D3 D3 D3  (BASIC)       │
├────────────────────────────────────────────────┤
│ [FILENAME: 6 bytes]                            │
│   Space-padded ASCII                           │
└────────────────────────────────────────────────┘
```

### 2.2 ASCII Files

`ASCII` files have a flexible structure and can span multiple blocks. Unlike `BINARY` and `BASIC` files which always use exactly two blocks, `ASCII` files can have as many data blocks as needed to store their content.

**End-of-File Detection:**

`ASCII` files use the byte `0x1A` (decimal 26) as an `EOF` marker. When the MSX reads an `ASCII` file, it stops reading at the first occurrence of this byte, treating it as the logical end of the file. 

**Block Size:**

`ASCII` data blocks are typically 256 bytes each. When creating CAS files, text is divided into 256-byte chunks. The last block must contain at least one `EOF` marker, and is padded to 256 bytes with `0x1A` bytes. If the text length is a multiple of 256, an additional block containing only `0x1A` padding is required.

**What happens after EOF:**
- Data after the `0x1A` marker within the same file is ignored (padding, garbage data)
- A `CAS HEADER` after the `EOF` marker may signal the start of a **new file** (not continuation of the current ASCII file)

**Important limitation:** Because `0x1A` serves as the `EOF` marker, ASCII files cannot contain this byte as part of their actual content. This is only a restriction for `ASCII` files—`BINARY` and `BASIC` files can include `0x1A` as regular data since they don't use an `EOF` marker.

**Putting it all together**

```
┌────────────────────────────────────────────────────────────────┐
│ ASCII FILE: "README" spanning 3 blocks                         │
└────────────────────────────────────────────────────────────────┘
Offset: 0x0000
┌────────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                              │
│   1F A6 DE BA CC 13 7D 74                                      │
└────────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 1: File Header Block (16 bytes)                          │
├────────────────────────────────────────────────────────────────┤
│ [TYPE MARKER: 10 bytes]                                        │
│   EA EA EA EA EA EA EA EA EA EA                                │
│ [FILENAME: 6 bytes]                                            │
│   52 45 41 44 4D 45                                            │
│   "README"                                                     │
└────────────────────────────────────────────────────────────────┘
Offset: 0x0018
┌────────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                              │
│   1F A6 DE BA CC 13 7D 74                                      │
└────────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 2: First Data Block (~256 bytes)                         │
├────────────────────────────────────────────────────────────────┤
│ [DATA: ~256 bytes of text content]                             │
│   54 68 69 73 20 69 73 20 61 20 6C 6F 6E 67 20 74 ...          │
│   "This is a long text file that spans multiple..."            │
│   "blocks. It contains documentation about the..."             │
│   "MSX cassette tape format. Since this content..."            │
│   ... (no 0x1A EOF marker in this block)                       │
└────────────────────────────────────────────────────────────────┘

Offset: 0x0120
┌────────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                              │
│   1F A6 DE BA CC 13 7D 74                                      │
└────────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 3: Second Data Block (with EOF)                          │
├────────────────────────────────────────────────────────────────┤
│ [DATA: continuation]                                           │
│   2E 2E 2E 63 6F 6E 74 69 6E 75 65 73 20 68 65 72 ...          │
│   "...continues here with more text content."                  │
│   "This is the end of the file."                               │
│ [EOF MARKER: 1 byte]                                           │
│   1A                  ← File ends here logically               │
│ [PADDING: ignored]                                             │
│   1A 1A 1A 1A ...     ← Everything after EOF ignored           │
└────────────────────────────────────────────────────────────────┘
```

**Key points:**
- File header block identifies type (EA) and name ("README")
- Data blocks contain actual text content
- No `EOF` marker in first data block → continue reading
- `EOF` marker (0x1A) in second data block → stop reading
- Any data after `EOF` is ignored (padding, garbage)
- If another `CAS HEADER` appears, check if it starts a NEW file


### 2.3 BASIC and BINARY Files

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

**Parsing approach:**

Since `BINARY/BASIC` files always have exactly 2 blocks (file header + data block), and the address calculation tells you the program data length, after reading the data header and the calculated number of program data bytes, the file is complete. 

After the program data, there may be padding bytes (`0x00`) to maintain 8-byte alignment. Since the next `CAS HEADER` must start at an 8-byte aligned offset, zero-byte padding is added if the program data doesn't end at an 8-byte boundary. To find the next file, scan forward to locate the next `CAS HEADER` at an aligned offset.

**No EOF Marker:**

Since these are binary files, all byte values from `0x00` to `0xFF` are valid data. There's no special `EOF` marker like ASCII's `0x1A`—the addresses define exactly where the data ends.

**File ID Bytes:**

When BINARY and BASIC files are stored on disk (e.g., in MSX-DOS), they include a 1-byte file ID prefix:
- **BINARY files:** `0xFE` prefix byte (not present in CAS format)
- **BASIC files:**  `0xFF` prefix byte (not present in CAS format)

These ID bytes are **NOT** stored in CAS files—they belong to the disk file format. When extracting BINARY files from CAS, tools typically add the `0xFE` prefix to match the disk format. When adding BINARY/BASIC files to a CAS, tools automatically strip these prefix bytes if present.

**Example: Adding prefix when extracting BINARY file from CAS to disk**

```
CAS data block (data header + program):
00 C0 00 C2 00 C0  21 00 C0 CD 00 00 C9 ...
└───────────────┘  └─────────────────────┘
   Addresses        Program data
   (6 bytes)

Disk file created:
FE 00 C0 00 C2 00 C0  21 00 C0 CD 00 00 C9 ...
└┘ └───────────────┘  └─────────────────────┘
0xFE   Addresses        Program data
prefix (6 bytes)
```

**Example: Removing prefix when adding BASIC file from disk to CAS**

```
Disk file (with 0xFF prefix):
FF 00 80 1A 81 00 80 00 00 0A 00 91 20 22 48 ...
└┘ └─────────────────┘  └─────────────────────────┘
0xFF   Addresses         Tokenized BASIC
prefix (6 bytes)

CAS data block (prefix stripped):
00 80 1A 81 00 80 00 00 0A 00 91 20 22 48 ...
└─────────────────┘  └─────────────────────────┘
   Addresses          Tokenized BASIC
   (6 bytes)
```

**Automatic Prefix Detection:**

Tools can reliably detect and remove the prefix byte by checking the first byte of disk files:
- If first byte is `0xFE` → BINARY file with prefix → skip first byte when adding to CAS
- If first byte is `0xFF` → BASIC file with prefix → skip first byte when adding to CAS
- Otherwise → Data starts immediately → use as-is

This simple check is reliable because:
1. MSX programs typically load at 0x8000-0xF000 range
2. Load addresses starting with 0xFE/0xFF would mean loading at 0xFE00+ (BIOS area)
3. Such high addresses are practically never used for program load addresses

**Putting it all together**

```
┌────────────────────────────────────────────────────────────────┐
│ BINARY FILE: "LOADER" with 512 bytes of Z80 code               │
└────────────────────────────────────────────────────────────────┘
Offset: 0x0000
┌────────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                              │
│   1F A6 DE BA CC 13 7D 74                                      │
└────────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 1: File Header Block (16 bytes)                          │
├────────────────────────────────────────────────────────────────┤
│ [TYPE MARKER: 10 bytes]                                        │
│   D0 D0 D0 D0 D0 D0 D0 D0 D0 D0                                │
│   (BINARY marker)                                              │
│ [FILENAME: 6 bytes]                                            │
│   4C 4F 41 44 45 52                                            │
│   "LOADER"                                                     │
└────────────────────────────────────────────────────────────────┘
Offset: 0x0018
┌────────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                              │
│   1F A6 DE BA CC 13 7D 74                                      │
└────────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────────┐
│ BLOCK 2: Data Block                                            │
├────────────────────────────────────────────────────────────────┤
│ [DATA HEADER: 6 bytes]                                         │
│   00 C0                   LOAD ADDRESS → 0xC000                │
│   FD C0                   END ADDRESS  → 0xC0FD                │
│   00 C0                   EXEC ADDRESS → 0xC000                │
│   Length = 0xC0FD - 0xC000 = 253 bytes                         │
│ [PROGRAM DATA: 253 bytes]                                      │
│   21 00 C0 CD 00 00 C9 ...                                     │
│   (Z80 machine code - 253 bytes)                               │
│ [PADDING: 5 bytes]                                             │
│   00 00 00 00 00 ← Zero padding for 8-byte alignment           │
│   (6 + 253 + 5 = 264 bytes = 33 × 8)                           │
└────────────────────────────────────────────────────────────────┘
```

**Key points:**
- Exactly 2 blocks, no more, no less
- Block 1: Identifies file type (D0 for BINARY) and name
- Block 2: Contains addresses and all program data
- Data length determined by END_ADDRESS - LOAD_ADDRESS
- All bytes (including 0x1A) are valid data
- Zero-byte padding (`0x00`) added if needed for 8-byte alignment
- After reading 2nd block, file is complete → next `CAS HEADER`
  starts a NEW file (if present)

```
┌────────────────────────────────────────────────────────────┐
│ BASIC FILE: "GAME  " with tokenized BASIC program          │
└────────────────────────────────────────────────────────────┘
Offset: 0x0000
┌────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                          │
│   1F A6 DE BA CC 13 7D 74                                  │
└────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────┐
│ BLOCK 1: File Header Block (16 bytes)                      │
├────────────────────────────────────────────────────────────┤
│ [TYPE MARKER: 10 bytes]                                    │
│   D3 D3 D3 D3 D3 D3 D3 D3 D3 D3                            │
│   (BASIC marker)                                           │
│ [FILENAME: 6 bytes]                                        │
│   47 41 4D 45 20 20                                        │
│   "GAME  " (space-padded)                                  │
└────────────────────────────────────────────────────────────┘
Offset: 0x0018
┌────────────────────────────────────────────────────────────┐
│ [CAS HEADER: 8 bytes] ← Delimiter                          │
│   1F A6 DE BA CC 13 7D 74                                  │
└────────────────────────────────────────────────────────────┘
         ↓ Marks start of block
┌────────────────────────────────────────────────────────────┐
│ BLOCK 2: Data Block                                        │
├────────────────────────────────────────────────────────────┤
│ [DATA HEADER: 6 bytes]                                     │
│   00 80                   LOAD ADDRESS → 0x8000            │
│   1A 81                   END ADDRESS  → 0x811A            │
│   00 80                   EXEC ADDRESS → 0x8000            │
│   Length = 0x811A - 0x8000 = 282 bytes                     │
│                                                            │
│ [BASIC PROGRAM: 282 bytes in tokenized format]             │
│   (Internal tokenized format structure)                    │
└────────────────────────────────────────────────────────────┘
```

**Key points:**
- BASIC programs are stored in tokenized format, not ASCII text
- Keywords become single-byte tokens (e.g., PRINT=0x91, END=0x81, FOR=0x82)
- Line numbers and program structure are encoded in binary format
- Even if program contains 0x1A byte, it's treated as data, not `EOF`
- Zero-byte padding (`0x00`) added if needed for 8-byte alignment

## 3. MSX Tape Encoding

This section describes how MSX computers actually read and write cassette tapes. While CAS files store the logical structure, real MSX hardware converts data into audio signals that can be recorded on magnetic tape.

### 3.1 FSK Modulation

MSX uses Frequency Shift Keying (FSK) to convert digital bits into audio tones. Each bit value is represented by a specific frequency, allowing ordinary cassette recorders to store computer data.

**Transmission:** Data is encoded as 1200 Hz or 2400 Hz tones  
**Detection:** MSX reads the tape by measuring zero-crossing timing (see section 4.2)

Supported baud rates: 1200 baud (default) or 2400 baud.

**Common WAV conversion settings**:
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

The MSX doesn't directly measure frequencies. Instead, it measures the time between zero crossings (when the signal crosses zero amplitude) by counting CPU T-states.

**Zero crossing intervals at 1200 baud:**
- 1200 Hz: half-cycle = 416.7 µs = 1491 T-states (`LONG` = 0-bit)
- 2400 Hz: half-cycle = 208.3 µs = 746  T-states (`SHORT` = 1-bit)

1. **Wait for zero crossing** - Detect when signal crosses zero
2. **Start timer** - Begin counting CPU T-states (CPU cycles)
3. **Wait for next zero crossing** - One half-cycle later
4. **Stop timer** - End measurement
5. **Compare with thresholds** - If time is `SHORT`, it's a 1-bit. If `LONG`, it's a 0-bit

**Why different cycle counts:** The FSK encoding deliberately uses 1 cycle at 1200 Hz for 0-bits and 2 cycles at 2400 Hz for 1-bits. Both bit types take the same time duration (833.3 µs at 1200 baud), but 1-bits contain twice as many wave cycles at twice the frequency. The MSX detects which bit was sent by measuring the time between zero crossings—a longer interval indicates a 0-bit, while a shorter interval indicates a 1-bit.

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

**Sequence:** silence → sync pulses → data bytes

- **First block of each file** (file header block):
  - Long silence: 2 seconds (motor startup and stabilization)
  - Initial sync: 8000 1-bits (~6.67 sec) (baud rate detection and carrier lock)
  - Data: Type marker (10 bytes) + Filename (6 bytes) = 16 bytes total

- **Subsequent blocks within the same file** (data blocks):
  - Short silence: 1 second (inter-block gap)
  - Block sync: 2000 1-bits (~1.67 sec) (re-synchronization)
  - Data: Block content (addresses + program data, or text data)

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
│   Each byte: START(0) + 8 data + 2 STOP(2x1)                │
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

### 3.5 Mapping CAS Structure to Audio Encoding

This section shows how the logical CAS file structure (discussed in Section 2) maps to the physical audio encoding on tape:

```
┌─────────────────────────────────────────────────────────────┐
│ CAS FILE STRUCTURE          AUDIO ENCODING                  │
├─────────────────────────────────────────────────────────────┤
│ [CAS HEADER: 8 bytes]  →    [Long silence: 2 sec]           │
│ (File header delimiter)     [Initial sync: 8000 1-bits]     │
│                             [8 bytes encoded as audio]      │
│                                                             │
│ File header block:     →    [16 bytes encoded as audio:     │
│   Type marker (10 B)          - Type marker (10 bytes)      │
│   Filename (6 B)              - Filename (6 bytes)]         │
├─────────────────────────────────────────────────────────────┤
│ [CAS HEADER: 8 bytes]  →    [Short silence: 1 sec]          │
│ (Data block delimiter)      [Block sync: 2000 1-bits]       │
│                             [8 bytes encoded as audio]      │
│                                                             │
│ Data block:            →    [Data encoded as audio:         │
│   (varies by type)            - Addresses (6 bytes)         │
│                               - Program/text data           │
│                               - Padding if needed]          │
├─────────────────────────────────────────────────────────────┤
│ [Next file...]         →    [Long silence: 2 sec]           │
│                             [Initial sync: 8000 1-bits]     │
│                             ...                             │
└─────────────────────────────────────────────────────────────┘
```

**Key points:**
- First block of each file gets long silence (2 sec) + initial sync (8000 1-bits) + data bytes
- Subsequent blocks get short silence (1 sec) + block sync (2000 1-bits) + data bytes
- All data bytes (type markers, filenames, addresses, program data, text) are encoded using FSK with serial framing (START + 8 data bits + 2 STOP bits)
- The silence and sync are **not** stored in CAS files—they're added during WAV conversion or tape recording

---

## 4. Implementation Guide

### 4.1 CAS to WAV Conversion

To convert a CAS file to audio (WAV format), you need to parse the CAS structure and generate the corresponding audio pulses:

**Algorithm:**

1. **Parse CAS file** - Scan for `CAS HEADER` delimiters (`1F A6 DE BA CC 13 7D 74`)
2. **Determine block type** - Check if block is a file header (by examining the type marker)
3. **Generate silence and sync:**
   - **File header blocks:** Long silence (2 sec) + Initial sync (8000 1-bits)
   - **Data blocks:** Short silence (1 sec) + Block sync (2000 1-bits)
4. **Encode data bytes** - Convert each byte using FSK with serial framing:
   - START bit (0-bit) = 1 cycle at 1200 Hz
   - 8 DATA bits (LSB first) = 1-bit as 2 cycles at 2400 Hz, 0-bit as 1 cycle at 1200 Hz
   - 2 STOP bits (1-bits) = 2 cycles at 2400 Hz each
5. **Repeat** for each block in the CAS file
6. **Write WAV file** - Output as PCM audio (typically 43200 Hz sample rate, 8-bit mono)

**Key points:**
- The 8-byte `CAS HEADER` itself is **not** encoded as audio—it triggers the silence/sync sequence
- First `CAS HEADER` of each file → long silence + initial sync
- Subsequent `CAS HEADER`s → short silence + block sync
- All block data (type markers, filenames, addresses, program bytes) are encoded as audio

### 4.2 WAV to CAS Conversion (wav2cas algorithm)

To convert audio (WAV format) back to a CAS file, you need to decode the audio pulses and reconstruct the CAS structure. This is the algorithm used by the wav2cas tool:

**Algorithm:**

1. **Skip silence** - Advance past low-amplitude regions (below threshold)
2. **Detect sync header** 
   - Find sequence of 25+ similar-width pulses using zero-crossing detection
   - Calculate average pulse width for adaptive byte decoding
3. **Write CAS HEADER** - Pad to 8-byte alignment, then write delimiter: `1F A6 DE BA CC 13 7D 74`
4. **Decode bytes** - Convert audio pulses to bytes:
   - Measure zero-crossing intervals to distinguish 1200 Hz (0-bit) from 2400 Hz (1-bit)
   - Extract serial frame: START (0-bit) + 8 DATA bits (LSB first) + 2 STOP (1-bits)
   - Write each decoded byte to CAS file
5. **Continue until silence** - Repeat step 4 until silent region detected
6. **Repeat** - Return to step 1 until end of audio

**Key points:**
- When converting WAV→CAS: audio silence and sync sequences are NOT stored in the CAS file; instead a `CAS HEADER` delimiter is written
- When converting CAS→WAV: each `CAS HEADER` delimiter triggers generation of audio silence + sync sequence
- Uses adaptive pulse width tolerance (window factor ~1.5×) to handle tape speed variations
- Signal processing options: amplitude normalization, envelope correction (noise reduction), phase shifting
- Configurable thresholds allow tuning for different tape quality and recording conditions

### 4.3 Practical Limits

**Filename length:** Exactly 6 bytes. Longer names are automatically truncated. Shorter names are space-padded on the right (0x20). Names with fewer than 6 printable characters are right-padded with spaces. When extracting files, trailing spaces and null bytes are stripped.

**Filename character support:** ASCII characters only. The format treats filenames as 6-byte ASCII arrays. Characters outside printable ASCII range may cause issues.

**Block alignment:** Block data must be 8-byte aligned, which means `CAS HEADER`s (being 8 bytes) must be placed at 8-byte aligned offsets (0, 8, 16, 24, ...).

When creating CAS files, data blocks are padded with:
- **BINARY/BASIC files:** Zero bytes (0x00) for 8-byte alignment
- **ASCII files:** `EOF` bytes (0x1A) for 256-byte alignment
- **Custom blocks:** Zero bytes (0x00) for 8-byte alignment

**Loading time at 1200 baud:**
- 1 KB file: approximately 10 seconds
- 16 KB file: approximately 2.5 minutes
- These times include sync pulses and inter-block gaps

**Memory constraints:** MSX systems typically have 8-64 KB of RAM, so most files are small by modern standards.

**Basic File length:** For BASIC files: tokenized program data must be at least 2 bytes (minimal valid program)

---

## 5. Glossary

**ASCII file**  
A text file stored in a CAS container. ASCII files can span multiple data blocks and are terminated by a `0x1A` (`EOF`) byte. The MSX reads ASCII files character-by-character until encountering the `EOF` marker. Common for BASIC program listings saved with `SAVE "CAS:filename",A`.

**Baud rate**  
The transmission speed in bits per second. MSX cassette systems support 1200 baud (default) or 2400 baud. At 1200 baud, each bit takes 833.3 µs and approximately 109 bytes/second can be transmitted. The baud rate determines the frequency of audio pulses used in FSK encoding.

**BASIC file**  
A tokenized BASIC program stored in binary format. BASIC files always consist of exactly two blocks: a file header block and one data block. The data includes a 6-byte address header (load address, end address, and execution address) followed by the tokenized program where keywords are converted to single-byte tokens (e.g., `PRINT` becomes `0x91`). The address structure is identical to BINARY files. Loaded with the `LOAD "CAS:filename"` command, which recognizes the BASIC type marker (0xD3) and loads the tokenized program into BASIC's program area.

**BINARY file**  
A raw machine code program or data block stored with address information. BINARY files always consist of exactly two blocks: a file header block and one data block containing a 6-byte address header (load address, end address, and execution address) followed by the program bytes. Loaded into memory with MSX-BASIC's `BLOAD` command and optionally executed with the `,R` parameter.

**Block**  
A unit of data in a CAS file, delimited by `CAS HEADER`s. Each block contains either file metadata (type marker and filename) or actual file content (program data, text, etc.). ASCII files can have many data blocks, while BINARY and BASIC files always have exactly one data block following their header block.

**`CAS HEADER`**  
An 8-byte delimiter pattern (`1F A6 DE BA CC 13 7D 74`) that marks the beginning of every block in a CAS file. These headers allow parsers to locate block boundaries by scanning for this magic number. The `CAS HEADER` is not part of the actual file data—it's purely a structural marker in the CAS container format.

**Data block**  
A block containing the actual file content (as opposed to a file header block which contains metadata). For ASCII files, data blocks contain text. For BINARY and BASIC files, the data block contains a 6-byte address header followed by program data.

**Data header**  
The 6-byte address structure at the beginning of BINARY and BASIC data blocks. Contains three 16-bit little-endian values: load address (where data will be loaded in memory), end address (end of data range), and execution address (where to start execution with `BLOAD ...,R`). The program data length is calculated as `end_address - load_address`.

**8-byte alignment**  
A critical requirement of the CAS format: all blocks must start at offsets divisible by 8. Since `CAS HEADER`s are 8 bytes, this means they must appear at positions 0, 8, 16, 24, etc. When creating CAS files, data blocks are padded with zeros (BINARY/BASIC) or `0x1A` bytes (ASCII) to maintain this alignment.

**File header block**  
The first block of every file in a CAS container. Contains a 10-byte type marker (identifying the file as `ASCII`, `BASIC`, or `BINARY`) followed by a 6-byte filename. The MSX BIOS reads this block to determine how to process subsequent data blocks.

**FSK (Frequency Shift Keying)**  
The audio encoding method used by MSX cassette tapes to convert digital bits into audio tones. Zero bits are encoded as 1200 Hz (1 cycle per bit), while one bits are encoded as 2400 Hz (2 cycles per bit). The MSX hardware detects these frequencies by measuring the time between zero-crossings of the audio waveform.

**Logical EOF**  
The semantic end of an ASCII file, marked by the first occurrence of the `0x1A` byte. When the MSX reads an ASCII file, it stops at this marker and treats any subsequent data as padding or garbage. This is distinct from structural delimiters like `CAS HEADER`s—`0x1A` has meaning only within ASCII file content.

**Serial framing**  
The bit structure used to transmit each byte: 1 START bit (0-bit), 8 DATA bits (LSB first), and 2 STOP bits (1-bits), totaling 11 bits per byte. This is similar to RS-232 serial communication and allows the receiving hardware to synchronize on byte boundaries.

**Sync header**  
A sequence of consecutive 1-bits (2400 Hz pulses) transmitted before each data block to allow the MSX to detect carrier signal and synchronize timing. Initial sync (8000 1-bits, ~6.67 sec) precedes file header blocks. Block sync (2000 1-bits, ~1.67 sec) precedes data blocks. During WAV to CAS conversion, sync headers trigger insertion of `CAS HEADER` delimiters.

**Type marker**  
A 10-byte pattern in the file header block that identifies the file type: `0xEA` repeated 10 times for ASCII files, `0xD0` repeated 10 times for BINARY files, or `0xD3` repeated 10 times for BASIC files. This tells the MSX BIOS and CAS parsing tools how to interpret the subsequent data blocks.

**Zero-crossing**  
The point where an audio waveform crosses zero amplitude (changes from positive to negative or vice versa). MSX hardware detects bits by measuring the time interval between zero-crossings: longer intervals indicate 1200 Hz (0-bit), shorter intervals indicate 2400 Hz (1-bit). This timing-based method is more reliable than frequency analysis for degraded tape signals.

---

## 6. Reference

### Tools

**CASTools** (C implementation)
- Repository: https://github.com/joyrex2001/castools (original by Vincent van Dam)
- Fork: https://github.com/xesco/castools (this project)
- Utilities: cas2wav, wav2cas, casdir
- First release: 2001, latest version: 1.31 (2016)
- License: GPL-2.0

**MCP - MSX CAS Packager** (Rust implementation)
- Repository: https://github.com/apoloval/mcp
- Author: Alvaro Polo
- Features: Create, extract, list, and export CAS files to WAV
- License: Mozilla Public License 2.0

### Technical Standards

**Kansas City Standard**
- Wikipedia: https://en.wikipedia.org/wiki/Kansas_City_standard
- Basis for MSX tape encoding using FSK (Frequency Shift Keying)
- MSX uses 1200 baud variation with optional 2400 baud mode

### Community Resources

**MSX Community Sites**
- MSX Resource Center: https://www.msx.org/
- MSX Wiki: https://www.msx.org/wiki/
- Generation MSX: https://www.generation-msx.nl/

**Emulators for Testing**
- openMSX: https://openmsx.org/ (most accurate MSX emulator)
- blueMSX: http://www.bluemsx.com/
- WebMSX: https://webmsx.org/ (browser-based)

### Acknowledgments

This documentation was compiled from:
- Analysis of CASTools (by Vincent van Dam) and MCP (by Alvaro Polo) source code implementations
- Practical testing with MSX emulators
- MSX community knowledge and reverse engineering of existing CAS files
