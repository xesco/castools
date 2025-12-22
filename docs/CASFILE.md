# MSX CAS File Format Specification

## Implementation Guide and Technical Reference

This document describes the CAS container format and MSX cassette tape encoding protocol for developers implementing tape utilities and emulators. Information gathered from community knowledge, existing code implementations, and practical analysis.

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

File header block identifying file type and name.

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

`ASCII` files are used to store plain text data. They have a flexible structure and can span multiple blocks. Unlike `BINARY` and `BASIC` files which always use exactly two blocks, `ASCII` files can have as many data blocks as needed to store their content.

**End-of-File Detection:**

`ASCII` files use the byte `0x1A` (decimal 26) as an `EOF` marker. When the MSX reads an `ASCII` file, it stops reading at the first occurrence of this byte, treating it as the logical end of the file. 

**Block Size:**

`ASCII` data blocks are typically 256 bytes each. When creating CAS files, text is divided into 256-byte chunks. The last block must contain at least one `EOF` marker, and is padded to 256 bytes with `0x1A` bytes. If the text length is a multiple of 256, an additional block containing only `0x1A` padding is required.

**What happens after EOF:**
- Data after the `0x1A` marker within the same file is ignored (padding, garbage data)
- A `CAS HEADER` after the `EOF` marker may signal the start of a **new file** (not continuation of the current ASCII file)

**Putting it all together**

ASCII text file with multi-block structure and EOF marker.

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

**File ID Bytes:**

When `BINARY` and `BASIC` files are stored on disk (e.g., in MSX-DOS), they include a 1-byte file ID prefix:
- **BINARY files:** `0xFE` prefix byte (not present in CAS format)
- **BASIC files:**  `0xFF` prefix byte (not present in CAS format)

These ID bytes are **NOT** stored in CAS files—they belong to the disk file format. When extracting `BINARY` files from CAS, tools typically add the `0xFE` prefix to match the disk format. When adding `BINARY/BASIC` files to a CAS, tools automatically strip these prefix bytes if present.

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
- If first byte is `0xFE` → `BINARY` file with prefix → skip first byte when adding to CAS
- If first byte is `0xFF` → `BASIC` file with prefix → skip first byte when adding to CAS
- Otherwise → Data starts immediately → use as-is

This simple check is reliable because:
1. MSX programs typically load at 0x8000-0xF000 range
2. Load addresses starting with 0xFE/0xFF would mean loading at 0xFE00+ (top of address space)
3. Such high addresses are practically never used for program load addresses

**Putting it all together**

BINARY machine code file with address header and padding.

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

BASIC tokenized program with address header.

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
│ [NO PADDING NEEDED]                                        │
│   (6 + 282 = 288 bytes = 36 × 8, already aligned)          │
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

Data is encoded as either 1200 Hz (0 bit) or 2400 Hz (1 bit) audio tones. The MSX reads the tape by measuring zero-crossing timing (the intervals when the audio signal crosses zero amplitude) rather than directly measuring frequencies. The system supports two baud rates: 1200 baud (default) or 2400 baud.

**Note:** Examples in this document use 1200 baud, but conversion to 2400 baud is straightforward—simply halve all timing values and double all frequencies.

### 3.2 Bit Encoding

MSX encodes binary data by representing each bit (0 or 1) as a specific audio waveform pattern. The key principle is that **0-bits and 1-bits take the same amount of time, but use different frequencies**:

- **0-bit**: Encoded as 1 complete wave cycle at the lower frequency (1200 Hz)
- **1-bit**: Encoded as 2 complete wave cycles at double the frequency (2400 Hz)

This means a 1-bit has twice as many zero-crossings in the same time period as a 0-bit, allowing the MSX hardware to distinguish them by measuring the time between zero-crossings (longer intervals = 0, shorter intervals = 1).

| Baud Rate | Bit Type | Frequency | Full Cycle Time | Half-Cycle Time | T-States | Detection |
|-----------|----------|-----------|-----------------|-----------------|----------|-----------|
| 1200      | 0-bit    | 1200 Hz   | 833.3 µs        | 416.7 µs        | 1491     | `LONG`    |
| 1200      | 1-bit    | 2400 Hz   | 833.3 µs        | 208.3 µs        | 746      | `SHORT`   |
| 2400      | 0-bit    | 2400 Hz   | 416.7 µs        | 208.3 µs        | 746      | `LONG`    |
| 2400      | 1-bit    | 4800 Hz   | 416.7 µs        | 104.2 µs        | 373      | `SHORT`   |

**MSX bit detection algorithm:**

The MSX BIOS uses this zero-crossing timing method to decode bits from the cassette input signal. The process is **continuous**—the MSX repeatedly measures the time between consecutive zero-crossings:

1. **Wait for zero crossing** - Detect when signal crosses zero
2. **Start timer** - Begin counting CPU T-states (CPU cycles)
3. **Wait for next zero crossing** - One half-cycle later
4. **Stop timer** - End measurement
5. **Compare with thresholds** - If time is `SHORT`, it's part of a 1-bit. If `LONG`, it's part of a 0-bit
6. **Repeat** - Go back to step 1 to measure the next half-cycle

Since a 0-bit has 1 complete cycle (2 half-cycles = 2 LONG intervals) and a 1-bit has 2 complete cycles (4 half-cycles = 4 SHORT intervals), the MSX accumulates these measurements to decode each complete bit. The serial framing (START bit, 8 data bits, 2 STOP bits) provides byte-level synchronization—the START bit signals the beginning of a new byte, allowing the MSX to group the measured intervals into the correct bit sequence.

**Common WAV conversion settings**:

The 43200 Hz sample rate divides evenly into both 1200 Hz and 2400 Hz frequencies, making it mathematically clean to generate the FSK waveforms. The 8-bit depth is sufficient since MSX only needs to detect zero-crossings rather than precise amplitude values.

- Sample rate: 43200 Hz (how many times per second the audio is sampled)
- Bit depth: 8-bit unsigned PCM (256 discrete amplitude levels per sample, 0-255)
- Channels: Mono

**At 43200 Hz sample rate:**

| Baud Rate | Bit Value | Frequency   | Samples per Bit   |
|-----------|-----------|-----------  |-------------------|
| 1200      | 0-bit     | 1 × 1200 Hz | 36 samples        |
| 1200      | 1-bit     | 2 × 2400 Hz | 36 samples (2×18) |
| 2400      | 0-bit     | 1 × 2400 Hz | 18 samples        |
| 2400      | 1-bit     | 2 × 4800 Hz | 18 samples (2×9)  |

### 3.3 Serial Framing

Each byte is transmitted with start and stop bits, similar to RS-232 serial communication. This framing allows the receiving hardware to synchronize on byte boundaries.

**Frame structure:** 

START (0-bit) + 8 DATA bits (LSB first) + 2 STOP (1-bits) = 11 bits/byte

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

### 4.2 WAV to CAS Conversion:

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
- Uses adaptive pulse width tolerance to handle tape speed variations: accepts pulses within approximately ±50% of the expected duration to accommodate cassette motor speed fluctuations

### 4.3 Practical Limits

**Filename length:** Exactly 6 bytes. Longer names are automatically truncated. Shorter names are space-padded on the right (0x20). Names with fewer than 6 printable characters are right-padded with spaces. When extracting files, trailing spaces and null bytes are stripped.

**Filename character support:** ASCII characters only. The format treats filenames as 6-byte ASCII arrays. Characters outside printable ASCII range may cause issues.

**Block alignment:** Block data must be 8-byte aligned, which means `CAS HEADER`s (being 8 bytes) must be placed at 8-byte aligned offsets (0, 8, 16, 24, ...).

**Basic File length:** For BASIC files: tokenized program data must be at least 2 bytes (minimal valid program)

---

## 5. Glossary

**ASCII file**  
An MSX file type for text data. Can refer to: (1) the MSX file format identified by type marker `0xEA`, or (2) how such files are stored in CAS containers (multiple data blocks terminated by `0x1A` EOF byte). Created with `SAVE "CAS:filename",A`.

**Baud rate**  
The transmission speed in bits per second. MSX supports 1200 baud (default) or 2400 baud.

**BASIC file**  
A tokenized BASIC program identified by type marker `0xD3`. Consists of two blocks: file header and data block (6-byte address header + tokenized program). Keywords are converted to single-byte tokens (e.g., `PRINT` = `0x91`). Loaded with `LOAD "CAS:filename"`.

**BINARY file**  
Machine code program identified by type marker `0xD0`. Consists of two blocks: file header and data block (6-byte address header + program bytes). Loaded with `BLOAD "CAS:filename"` command.

**Block**  
A unit of data in a CAS file, delimited by `CAS HEADER`s. Contains either file metadata (file header block) or actual content (data block).

**`CAS HEADER`**  
An 8-byte delimiter (`1F A6 DE BA CC 13 7D 74`) marking the start of each block in a CAS file. Structural marker only, not part of file data.

**Data block**  
A block containing the actual file content (as opposed to a file header block which contains metadata). For ASCII files, data blocks contain text. For BINARY and BASIC files, the data block contains a 6-byte address header followed by program data.

**Data header**  
The 6-byte address structure in `BINARY` and `BASIC` data blocks. Contains three 16-bit little-endian values: load address, end address, and execution address.

**8-byte alignment**  
CAS format requirement: all blocks must start at offsets divisible by 8 (positions 0, 8, 16, 24, etc.). Data blocks are padded with zeros (`BINARY/BASIC`) or `0x1A` (`ASCII`).

**File header block**  
The first block of every file in a CAS container. Contains a 10-byte type marker (`0xEA`/`0xD0`/`0xD3`) and 6-byte filename.

**FSK (Frequency Shift Keying)**  
Audio encoding method for MSX cassette tapes. 0-bit = 1 cycle at 1200 Hz, 1-bit = 2 cycles at 2400 Hz. Detected by measuring zero-crossing intervals.

**Logical EOF**  
The end of an ASCII file, marked by the first `0x1A` byte. Data after this marker is ignored. Only meaningful for ASCII files, not structural like `CAS HEADER`s.

**Serial framing**  
Bit structure for transmitting bytes: 1 START bit (0) + 8 DATA bits (LSB first) + 2 STOP bits (1) = 11 bits per byte.

**Sync header**  
Sequence of consecutive 1-bits (2400 Hz pulses) before each block for synchronization. Initial sync: 8000 1-bits (file headers). Block sync: 2000 1-bits (data blocks).

**Type marker**  
A 10-byte pattern identifying file type: `0xEA` (ASCII), `0xD0` (BINARY), or `0xD3` (BASIC), each repeated 10 times.

**Zero-crossing**  
When audio waveform crosses zero amplitude. MSX detects bits by measuring intervals between crossings: longer = 1200 Hz (0-bit), shorter = 2400 Hz (1-bit).

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
