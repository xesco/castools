# MSX Cassette Tape & CAS File Format

## Technical Specification and Reference

A definitive, implementation-grade description of the MSX cassette system and the CAS container format, based on official MSX documentation, BIOS behavior, and real-world CAS analysis.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Conceptual Model (Layer Separation)](#2-conceptual-model-layer-separation)
3. [Glossary](#3-glossary)
4. [CAS File Format](#4-cas-file-format)
5. [CAS Block Delimiter](#5-cas-block-delimiter-cas-header)
6. [CAS File Structure](#6-cas-file-structure-global-view)
7. [File Header Block](#7-file-header-block)
8. [ASCII Files](#8-ascii-files)
9. [BASIC and BINARY Files](#9-basic-and-binary-files)
10. [Hex Dump Examples](#10-hex-dump-examples)
11. [Worked Example: Real CAS File Analysis](#11-worked-example-real-cas-file-analysis)
12. [Physical Tape Encoding (Audio Layer)](#12-physical-tape-encoding-audio-layer)
13. [Bit Encoding](#13-bit-encoding)
14. [Serial Framing](#14-serial-framing)
15. [Pulse Timing & Thresholds](#15-pulse-timing--thresholds)
16. [Sync and Silence (Audio)](#16-sync-and-silence-audio)
17. [MSX BIOS Tape Routines](#17-msx-bios-tape-routines)
18. [Cross-Reference: CAS Structure to BIOS Behavior](#18-cross-reference-cas-structure-to-bios-behavior)
19. [CAS vs WAV Relationship](#19-cas-vs-wav-relationship)
20. [Canonical Parsing Model](#20-canonical-parsing-model)
21. [Constraints and Limits](#21-constraints-and-limits)
22. [Common Mistakes](#22-common-mistakes)
23. [Scope and Non-Standard Formats](#23-scope-and-non-standard-formats)
24. [Tools and Software](#24-tools-and-software)

---

## 1. Introduction

The MSX cassette system was designed as a robust, timing-based storage protocol for unreliable analog tape.
The CAS file format is a lossless digital container that preserves the logical tape block structure without encoding audio timing.

**This document defines:**

- The CAS container format
- The MSX cassette audio protocol
- The exact semantics of ASCII, BASIC, and BINARY files
- How real MSX hardware and BIOS routines interpret data

*This is a specification, not an implementation guide.*

---

## 2. Conceptual Model (Layer Separation)

The MSX cassette system has two strictly separate layers:

```
+-------------------------------+
| Logical / Container Layer     |  → CAS files
+-------------------------------+
| Physical / Signal Layer       |  → Tape or WAV audio
+-------------------------------+
```

### Fundamental Rule

- **CAS describes structure**
- **Audio describes timing**
- **They must never be mixed**

---

## 3. Glossary

**CAS HEADER**  
8-byte block delimiter in CAS files.

**Block**  
Payload between two CAS headers.

**File header block**  
First block of a file; defines type and name.

**ASCII file**  
Text file terminated by `0x1A`.

**Binary file**  
Raw memory image loaded by BLOAD.

**Logical EOF**  
Semantic end of ASCII data (`0x1A`).

---

## 4. CAS File Format

A CAS file is a linear sequence of blocks.

**There is:**

- ❌ no directory
- ❌ no global header
- ❌ no length fields
- ❌ no checksum

Blocks are delimited by a fixed marker.

---

## 5. CAS Block Delimiter (CAS HEADER)

Every block begins with the same 8-byte header:

```
1F A6 DE BA CC 13 7D 74
```

**This marker:**

- ✅ exists only in CAS files
- ✅ never appears in audio
- ✅ is not data
- ✅ marks the start of a block

---

## 6. CAS File Structure (Global View)

```
CAS FILE
========

[CAS HEADER]
[BLOCK PAYLOAD]

[CAS HEADER]
[BLOCK PAYLOAD]

[CAS HEADER]
[BLOCK PAYLOAD]
...
```

**Block payload length is implicit:**

- it ends at the next CAS HEADER
- or at end-of-file

### Multi-File CAS Archives

Multiple files can be concatenated in a single CAS file:

```
MULTI-FILE CAS
==============

[CAS HEADER]     ← File 1 header block
[FILE HEADER: "GAME1 "]

[CAS HEADER]     ← File 1 data block(s)
[DATA...]

[CAS HEADER]     ← File 2 header block
[FILE HEADER: "GAME2 "]

[CAS HEADER]     ← File 2 data block(s)
[DATA...]
```

**Semantics:**

- Each file starts with its own header block (type marker + filename)
- Files are stored sequentially with no separator or index
- The BIOS CLOAD command reads files by scanning forward for matching filenames
- No global directory or file count exists
- Files are distinguished only by their type marker and name in the header block

---

## 7. File Header Block

Each logical file begins with a file header block.

```
FILE HEADER BLOCK
=================

+-------------------------------+
| CAS HEADER (8 bytes)          |
+-------------------------------+
| TYPE MARKER (10 bytes)        |
+-------------------------------+
| FILENAME (6 bytes, ASCII)     |
| padded with spaces or 00      |
+-------------------------------+
```

### Type Markers

| File type | Marker (10 bytes) |
|-----------|-------------------|
| ASCII     | `EA EA EA EA EA EA EA EA EA EA` |
| BINARY    | `D0 D0 D0 D0 D0 D0 D0 D0 D0 D0` |
| BASIC     | `D3 D3 D3 D3 D3 D3 D3 D3 D3 D3` |

---

## 8. ASCII Files

### Structure

- May span multiple data blocks
- Have no length field
- End logically at byte `0x1A`

```
ASCII FILE (MULTI-BLOCK)
=======================

[CAS HEADER]
[ASCII HEADER BLOCK]

[CAS HEADER]
[ASCII DATA BLOCK]
  text bytes...

[CAS HEADER]
[ASCII DATA BLOCK]
  text bytes...
  1A   ← EOF marker
  1A   ← padding (optional)
```

### EOF Semantics

- First `0x1A` = logical end of file
- `0x1A` is data, not a delimiter
- Any data after EOF is ignored
- CAS block boundaries are irrelevant to EOF

**Constraint:** ASCII files must not contain `0x1A` as valid content.

---

## 9. BASIC and BINARY Files

### Structure

BASIC and BINARY files always consist of **exactly two blocks:**

```
[CAS HEADER]
[FILE HEADER BLOCK]

[CAS HEADER]
[DATA BLOCK]
```

### Data Block (Conceptual)

```
+---------------------------+
| Load address (2 bytes)    |
| End address  (2 bytes)    |
| Exec address (2 bytes)    |
| Program bytes...          |
+---------------------------+
```

### Semantics

- Length is derived from addresses
- All byte values `00–FF` are valid
- `0x1A` has no special meaning

---

## 10. Hex Dump Examples

### Example 1: Simple Binary File

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

### Example 2: ASCII File (Multi-Block)

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

### Example 3: BASIC File with Header Details

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

### Example 4: Multi-File CAS

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

---

## 11. Worked Example: Real CAS Analysis

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

### This Confirms:

- EOF is semantic, not structural
- CAS headers are pure delimiters
- Multi-file CAS images are normal

---

## 12. Physical Tape Encoding (Audio Layer)

MSX uses **FSK (Frequency Shift Keying)**.

**Supported baud rates:**

- 1200 baud (default)
- 2400 baud

**Standard WAV encoding parameters:**

- Sample rate: **43200 Hz**
- Bit depth: 8-bit unsigned PCM
- Channels: Mono
- DC offset: 128 (represents zero amplitude)

---

## 13. Bit Encoding

### 1200 baud

| Bit | Encoding | Duration @ 43200 Hz |
|-----|----------|---------------------|
| 0   | 1 cycle @ 1200 Hz | 36 samples |
| 1   | 2 cycles @ 2400 Hz | 36 samples (18 per cycle) |

**Timing:** Each bit takes 1/1200 second = 833.3 µs

### 2400 baud

| Bit | Encoding | Duration @ 43200 Hz |
|-----|----------|---------------------|
| 0   | 1 cycle @ 2400 Hz | 18 samples |
| 1   | 2 cycles @ 4800 Hz | 18 samples (9 per cycle) |

**Timing:** Each bit takes 1/2400 second = 416.7 µs

### Byte Transmission Rate

| Baud Rate | Bits per Byte | Time per Byte | Bytes per Second |
|-----------|---------------|---------------|------------------|
| 1200      | 11 (1+8+2)    | 9.17 ms       | ~109 bytes/sec   |
| 2400      | 11 (1+8+2)    | 4.58 ms       | ~218 bytes/sec   |

---

## 14. Serial Framing

Each byte is transmitted as:

```
START  : 0
DATA   : 8 bits (LSB first)
STOP   : 1, 1
```

**Bit Polarity:**

- **START bit = 0-bit** (1200 Hz pulse)
- **STOP bits = 1-bit** (2400 Hz pulse × 2, transmitted twice)
- Data bits use 0 or 1 encoding as needed

**Total:** 11 bit times per byte

**Timing per byte at 1200 baud:**
- 1 START bit × 833.3 µs = 833.3 µs
- 8 DATA bits × 833.3 µs = 6666.4 µs
- 2 STOP bits × 833.3 µs = 1666.6 µs
- **Total: 9166.3 µs (~9.17 ms per byte)**

---

## 15. Pulse Timing & Thresholds

The MSX does not measure frequency.

**It measures:**

- time between zero crossings
- i.e. half-cycle duration

Pulse duration is compared against internal thresholds to classify:

- SHORT pulses
- LONG pulses

Thresholds differ between 1200 and 2400 baud.

---

## 16. Sync and Silence (Audio)

Before each block, audio includes:

```
[silence] → [sync pulses] → [data pulses]
```

**Purpose:**

- motor settling
- baud detection
- timing lock

These replace CAS headers at the audio level.

### Silence Durations

| Type | Duration | Samples @ 43200 Hz |
|------|----------|-------------------|
| **Long silence** | 2 seconds | 86,400 samples |
| **Short silence** | 1 second | 43,200 samples |

### Sync Pulse Counts (at 1200 baud)

| Type | Purpose | Pulse Count | Duration |
|------|---------|-------------|----------|
| **Initial sync** | First block of file | 8000 1-bits | ~6.67 seconds |
| **Block sync** | Between blocks | 2000 1-bits | ~1.67 seconds |

**Note:** At 2400 baud, pulse counts are doubled to maintain the same duration.

### Typical Audio Sequence

**For first block (file header):**
```
[2 sec silence] → [8000 sync pulses] → [header data] → [data bytes]
```

**For subsequent blocks:**
```
[1 sec silence] → [2000 sync pulses] → [data bytes]
```

---

## 17. MSX BIOS Tape Routines

The MSX BIOS provides standardized entry points for cassette tape operations. These routines handle the audio encoding/decoding automatically.

### Tape Input Routines

| Entry Point | Address | Function | Input | Output | Notes |
|-------------|---------|----------|-------|--------|-------|
| **TAPION** | #00E1 | Read header block and turn motor on | None | Carry flag set if failed | Initializes tape reading session |
| **TAPIN** | #00E4 | Read one byte from tape | None | A = byte read<br>Carry flag set if failed | Reads serial data |
| **TAPIOF** | #00E7 | Stop reading from tape | None | None | Turns off motor |

### Tape Output Routines

| Entry Point | Address | Function | Input | Output | Notes |
|-------------|---------|----------|-------|--------|-------|
| **TAPOON** | #00EA | Turn motor on and write header | A = #00 short header<br>A ≠ #00 long header | Carry flag set if failed | Writes sync and silence |
| **TAPOUT** | #00ED | Write one byte to tape | A = byte to write | Carry flag set if failed | Writes serial data |
| **TAPOOF** | #00F0 | Stop writing to tape | None | None | Turns off motor |

### Motor Control

| Entry Point | Address | Function | Input | Output | Notes |
|-------------|---------|----------|-------|--------|-------|
| **STMOTR** | #00F3 | Control cassette motor | A = #00 stop<br>A = #01 start<br>A = #FF toggle | None | Direct motor control |

### Header Types

- **Short header**: SYNC_BLOCK (2000 pulses @ 1200 baud = ~1.67 sec)
- **Long header**: SYNC_INITIAL (8000 pulses @ 1200 baud = ~6.67 sec)

### Usage Pattern

**Reading:**
```
CALL TAPION      ; Initialize, read header
loop:
  CALL TAPIN     ; Read byte into A
  JR C, error    ; Check for errors
  ; Process byte in A
  JR loop
error:
CALL TAPIOF      ; Stop motor
```

**Writing:**
```
LD A, #FF        ; Long header
CALL TAPOON      ; Initialize, write header
loop:
  LD A, byte     ; Get byte to write
  CALL TAPOUT    ; Write byte
  JR C, error    ; Check for errors
  JR loop
error:
CALL TAPOOF      ; Stop motor
```

---

## 18. Cross-Reference: CAS Structure to BIOS Behavior

This table maps CAS file structure to the corresponding MSX BIOS operations:

| CAS Structure Element | BIOS Routine | Audio Encoding | Purpose |
|----------------------|--------------|----------------|----------|
| CAS HEADER (`1F A6...`) | — | *Not transmitted* | Container delimiter only |
| Initial file block | TAPOON (long) | 2 sec silence + 8000 sync pulses | Motor startup, BIOS synchronization |
| File type marker (`EA`/`D0`/`D3` ×10) | TAPOUT × 10 | 10 bytes serial encoded | BIOS file type identification |
| Filename (6 bytes) | TAPOUT × 6 | 6 bytes serial encoded | BIOS file matching |
| Subsequent blocks | TAPOON (short) | 1 sec silence + 2000 sync pulses | Inter-block synchronization |
| Data bytes | TAPOUT | Serial: START + 8 bits + 2 STOP | Actual payload |
| EOF marker (`0x1A`) | TAPOUT | Normal byte encoding | Logical file termination (ASCII) |
| End of reading | TAPIOF | Motor stop | Session cleanup |
| End of writing | TAPOOF | Motor stop | Session cleanup |

### Key Mappings

**CAS Block Boundaries ↔ Audio Sync**
- Each CAS HEADER in file → One TAPOON call → Sync pulses in audio
- CAS has explicit markers → Audio uses timing and sync patterns

**File Type Detection**
- CAS: Type marker is first 10 bytes after CAS HEADER
- BIOS: TAPION reads and validates this marker
- Audio: Transmitted as normal serial data

**Data Encoding**
- CAS: Raw bytes (no encoding)
- BIOS: TAPOUT/TAPIN handle serial framing automatically
- Audio: FSK modulation with START/STOP bits

---

## 19. CAS vs WAV Relationship

**CAS (container)**
```
[CAS HEADER][BLOCK]
[CAS HEADER][BLOCK]
```

**WAV / Tape (audio)**
```
[silence]
[sync pulses]
[data pulses]
```

### Rules

- ✅ CAS headers never appear in audio
- ✅ Audio sync never appears in CAS
- ✅ CAS contains no timing information

---

## 20. Canonical Parsing Model

```
STATE: SEARCH_CAS_HEADER
  ↓
READ TYPE MARKER
  ↓
READ FILENAME
  ↓
READ DATA BLOCKS
  ↓
IF ASCII:
    stop logically at first 0x1A
IF BINARY/BASIC:
    read exactly one data block
  ↓
REPEAT
```

---

## 21. Constraints and Limits

### Technical Constraints

- Bit time is fixed per baud rate
- Stretching pulses increases load time
- Fastest standard speed: 2400 baud
- Faster loading requires non-standard loaders

### Practical Limits

| Parameter | Value | Notes |
|-----------|-------|-------|
| Filename length | 6 bytes | ASCII, space or null padded |
| Max file size | No limit | Limited by available tape/memory |
| Block alignment | 8 bytes | CAS format requirement |
| ASCII block size | 256 bytes | Typical, with EOF padding |

### Load Time Examples (1200 baud)

| File Size | Approximate Time |
|-----------|------------------|
| 1 KB      | ~10 seconds |
| 16 KB     | ~2.5 minutes |
| 32 KB     | ~5 minutes |

**Note:** Includes sync and silence overhead. 2400 baud is approximately twice as fast.

---

## 22. Common Mistakes

- ❌ Treating CAS headers as data
- ❌ Assuming one block equals one file
- ❌ Treating `0x1A` as structural EOF
- ❌ Expecting block length fields
- ❌ Mixing CAS parsing with audio decoding
- ❌ Assuming fixed block counts

**All of these are disproven by real CAS files.**

---

## 23. Scope and Non-Standard Formats

**Out of scope:**

- Turbo loaders
- Compressed formats
- Custom BIOS replacements
- Proprietary encodings

These require custom loaders and are not MSX standard.

---

## 24. Tools and Software

### CASTools (Original) - Vincent van Dam
- Original C implementation (legacy)
- cas2wav, wav2cas, casdir utilities
- Widely used reference implementation
- Source of many modern forks and reimplementations

### MCP (MSX CAS Packager) - Rust implementation
- Create CAS files from individual files (.bin, .bas, .asc)
- Extract files from CAS archives
- List CAS contents
- Export CAS to WAV
- Platform: Cross-platform (Rust)
- Features: Automatic padding, file validation, proper 8-byte alignment
- License: Mozilla Public License 2.0
- Repository: https://github.com/apoloval/mcp

---

## Final Statement

> **CAS is a structural container.**  
> **Tape is a timing protocol.**  
> **EOF is semantic, not structural.**  
> **Correct tools must respect this separation.**

---

*Document compiled from official MSX documentation, BIOS behavior analysis, and real-world CAS file examination.*
