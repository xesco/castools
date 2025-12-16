# MSX Cassette Tape & CAS File Format

## Technical Specification and Reference

A definitive, implementation-grade description of the MSX cassette system and the CAS container format, based on official MSX documentation, BIOS behavior, and real-world CAS analysis.

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

**Key distinction:** CAS preserves structure; tape encodes audio. They are separate concepts requiring conversion tools.

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

Every block begins with an 8-byte marker: `1F A6 DE BA CC 13 7D 74`

This exists only in CAS files, never in MSX tape audio.

### 3.2 File Structure

```
[CAS HEADER] [BLOCK PAYLOAD]
[CAS HEADER] [BLOCK PAYLOAD]
...
```

Block length is implicit (ends at next header or EOF). Multiple files can be concatenated sequentially with no global directory.

### 3.3 File Header Block

Structure: CAS HEADER + TYPE MARKER (10 bytes) + FILENAME (6 bytes, space-padded)

**Type markers:**
- ASCII: `EA` × 10
- BINARY: `D0` × 10  
- BASIC: `D3` × 10

### 3.4 ASCII Files

May span multiple blocks. End logically at first `0x1A` byte (EOF marker). Data after EOF is ignored. ASCII files cannot contain `0x1A` as valid content.

### 3.5 BASIC and BINARY Files

Always exactly two blocks: header + data.

Data block structure: Load address (2 bytes) + End address (2 bytes) + Exec address (2 bytes) + Program bytes.

Length derived from addresses. All byte values `00–FF` valid. No special meaning for `0x1A`.

---

## 4. MSX Tape Encoding

Official MSX standard for physical audio storage.

### 4.1 FSK Modulation

Uses Frequency Shift Keying at 1200 baud (default) or 2400 baud. WAV parameters: 43200 Hz, 8-bit unsigned PCM, mono.

### 4.2 Bit Encoding

**1200 baud:** 0-bit = 1×1200Hz (36 samples), 1-bit = 2×2400Hz (36 samples)  
**2400 baud:** 0-bit = 1×2400Hz (18 samples), 1-bit = 2×4800Hz (18 samples)

### 4.3 Serial Framing

START (0-bit) + 8 DATA bits (LSB first) + 2 STOP (1-bits) = 11 bits/byte

Timing at 1200 baud: ~9.17 ms per byte (~109 bytes/sec)

### 4.4 Sync and Silence

Before each block: silence → sync pulses → data

- Long silence: 2 sec (first block)
- Short silence: 1 sec (subsequent blocks)
- Initial sync: 8000 1-bits (~6.67 sec)
- Block sync: 2000 1-bits (~1.67 sec)

---

## 5. MSX BIOS Tape Routines

| Function | Address | Purpose |
|----------|---------|---------|
| TAPION | #00E1 | Read header and start motor |
| TAPIN | #00E4 | Read one byte |
| TAPIOF | #00E7 | Stop reading |
| TAPOON | #00EA | Write header and start motor |
| TAPOUT | #00ED | Write one byte |
| TAPOOF | #00F0 | Stop writing |
| STMOTR | #00F3 | Motor control |

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

```
SEARCH_CAS_HEADER → READ TYPE MARKER → READ FILENAME → READ DATA BLOCKS
IF ASCII: stop at first 0x1A
IF BINARY/BASIC: read one data block
REPEAT
```

### 7.2 Common Mistakes

❌ Treating CAS headers as data | ❌ Mixing CAS parsing with audio | ❌ Expecting length fields | ❌ Treating 0x1A as structural

### 7.3 Limits

Filename: 6 bytes | Block alignment: 8 bytes | Load time (1200 baud): 1KB ~10 sec, 16KB ~2.5 min

### 7.4 Scope

Covers MSX standard and CAS preservation. Out of scope: turbo loaders, compressed formats, custom BIOS.

---

## 8. Reference

### 8.1 Tools

**CASTools** (C) - cas2wav, wav2cas, casdir utilities  
**MCP** (Rust) - https://github.com/apoloval/mcp - Create, extract, list CAS files

### 8.2 Documentation

*Compiled from official MSX documentation, BIOS behavior analysis, and real-world CAS examination.*
