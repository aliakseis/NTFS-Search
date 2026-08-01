# NTFS-Search
Taken from https://sourceforge.net/projects/ntfs-search/

![20160818_151702_2](https://user-images.githubusercontent.com/11851670/177040370-794ed182-56c1-4078-b153-5eab196732bc.jpg)

A fast native Windows application for searching files directly from the NTFS Master File Table (MFT) without relying on the Windows Search service or filesystem enumeration.

Instead of recursively traversing directories, NTFS-Search reads NTFS metadata structures directly from the disk, allowing extremely fast filename searches even on very large volumes.

---

## Features

- Extremely fast filename search
- Direct NTFS Master File Table (MFT) parsing
- No dependency on Windows Search indexing
- Wildcard search support
- Multiple drive support
- Native Win32 GUI
- Displays full file paths
- Retrieves file size information
- Uses system file icons
- Handles very large numbers of search results
- Minimal memory overhead
- Works on Windows NTFS volumes without installing drivers

---

## How It Works

Unlike conventional search utilities that recursively enumerate directories using Win32 APIs, this application communicates directly with NTFS volumes.

The program:

1. Opens the physical NTFS volume.
2. Reads the NTFS boot sector.
3. Determines filesystem geometry.
4. Locates the Master File Table (MFT).
5. Sequentially parses MFT records.
6. Extracts filenames and directory relationships.
7. Builds complete file paths.
8. Matches filenames against wildcard expressions.
9. Displays matching results.

Because almost all directory information already exists inside the MFT, the search speed depends primarily on sequential disk throughput rather than filesystem traversal.

---

# Technical Details

## Raw NTFS Access

The application opens volumes directly using Windows device paths:

```
\\.\C:
```

It reads raw sectors with `CreateFile()` and `ReadFile()`, bypassing normal filesystem enumeration.

---

## Boot Sector Parsing

The NTFS boot sector is parsed to obtain:

- bytes per sector
- sectors per cluster
- bytes per cluster
- MFT starting cluster
- file record size

These values are then used to locate and decode MFT records correctly.

---

## Master File Table Parsing

The project implements its own NTFS parser capable of reading:

- FILE records
- resident attributes
- non-resident attributes
- filename attributes
- bitmap information
- attribute lists

Rather than depending on undocumented APIs, the parser works directly with on-disk NTFS structures.

---

## Filename Reconstruction

Every file record contains one or more filename attributes.

The application reconstructs complete paths by following parent directory references stored in the MFT.

This avoids directory enumeration entirely.

---

## Wildcard Matching

Filename filtering supports wildcard expressions including:

- `*`
- `?`

The project includes its own wildcard matching implementation for fast comparisons.

Examples:

```
*.cpp
*.txt
report*
image??.png
```

---

## Large Result Sets

Search results are stored inside STL containers and the application is designed to support very large result counts.

The code contains safeguards preventing excessive allocations while still allowing millions of discovered files.

---

## Memory Management

To reduce allocation overhead during MFT parsing the project implements custom heap structures.

Benefits include:

- fewer heap allocations
- reduced fragmentation
- faster parsing
- efficient storage of path strings
- efficient storage of filename strings

---

## User Interface

The application uses the native Win32 API.

Features include:

- standard Windows controls
- ListView results
- system image list
- progress reporting
- drive selection
- sorting support
- shell integration

Because no UI framework is used, the executable remains lightweight.

---

## Performance Characteristics

Searching an NTFS volume is typically much faster than recursive directory enumeration because:

- metadata is read sequentially
- almost no directory traversal is required
- minimal Win32 filesystem API overhead
- no dependency on Windows indexing

Performance primarily depends on:

- storage device speed
- MFT size
- number of files
- search pattern complexity

---

# Architecture

```
GUI
 │
 ├── Search Thread
 │
 ├── NTFS Volume Reader
 │
 ├── Boot Sector Parser
 │
 ├── MFT Reader
 │
 ├── Attribute Parser
 │
 ├── Filename Reconstruction
 │
 └── Search Filter
        │
        ▼
    Search Results
```

---

# Technologies

- C++
- Win32 API
- Native Windows Controls
- NTFS On-Disk Structures
- STL
- Multithreading
- Windows Shell API

---

# Requirements

- Windows
- NTFS-formatted drives
- Read access to the target volume
- Administrator privileges may be required for raw volume access depending on the Windows version and system configuration.

---

# Interesting Implementation Details

The project demonstrates a number of low-level Windows programming techniques, including:

- direct volume access
- NTFS filesystem parsing
- boot sector decoding
- Master File Table interpretation
- resident/non-resident attribute processing
- parent directory reconstruction
- custom memory allocation
- wildcard matching
- Win32 GUI programming
- shell icon extraction
- multithreaded searching
- handling millions of filesystem records efficiently

---

# Advantages

- Very fast searches
- Independent of Windows Search service
- No indexing required
- Low memory usage
- Lightweight native executable
- Educational implementation of NTFS internals
- Demonstrates raw filesystem programming techniques

---

# Educational Value

This project serves as an excellent reference for developers interested in:

- NTFS internals
- filesystem implementation
- raw disk access
- Windows systems programming
- Win32 GUI development
- metadata-based file searching
- low-level storage programming
- custom parsers
- performance-oriented native C++ programming

