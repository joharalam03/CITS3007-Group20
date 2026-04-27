# Phase 1 – BUN Parser Report

## Group Informaton

### **Group Number: 20 **  

### Group Members

1. **Name: Zawad Huda**  
   **Student Number: 23102177**  
   **GitHub Username:**  

2. **Name: Johar Khan**  
   **Student Number: 24331036**  
   **GitHub Username: joharalam03**  

3. **Name: Jessica Lin**  
   **Student Number: 24254044**  
   **GitHub Username: JessciaLinn**  

4. **Name: Mineth Perera**  
   **Student Number: 23284373**  
   **GitHub Username:**  

5. **Name: Synne Wikborg**  
   **Student Number: 24282424**  
   **GitHub Username: sjiang0**  

---

## 1. Output Format

Our parser prints decoded BUN file information in a human-readable text format.

For a valid file, output is written to **standard output**. The output contains:

- The BUN header fields
- One section for each asset record
- A preview of each asset name
- A preview of each asset payload

An example header is printed as follows

```text
BUN Header
magic: 0x304e5542
version_major: 1
version_minor: 0
asset_count: 1
asset_table_offset: 60
string_table_offset: 108
string_table_size: 8
data_section_offset: 116
data_section_size: 20
reserved: 0
```

and an example assest record is printed as follows

```text
Asset 0
name_offset: 0
name_length: 5
data_offset: 0
data_size: 18
uncompressed_size: 0
compression: 0
type: 0
checksum: 0
flags: 0
name_preview: hello
data_preview: Hello, BUN
```

Certain assest names and payloads may be long, so the parser will only display the initial part of the long values. For names, the first 60 printable characters are shown. For the payloads, printable ASCII data is shown as text. Binary data is shown as hexadecimal bytes.

For an invalid file, the parser prints any safely decoded header or asset information to standard output, then prints validation errors to standard error. Each violation is printed on a separate line:

```text
Violation invalid magic value
Violation asset 0 name length is zero
Violation asset 1 data range exceeds data section
```

There are certain errors that prevent futhur parsing such as if the file is shorter than the 60 byte header, the parser reports the error and stops.

The parser uses these exit codes

 Exit code | Meaning |
|---:|---|
| 0 | BUN_OK — file parsed successfully |
| 1 | BUN_MALFORMED— file violates the BUN specification |
| 2 | BUN_UNSUPPORTED — file uses an unsupported feature, such as zlib compression, unsupported version, non-zero checksum, or unsupported flag bits |
| 3 | BUN_ERR_IO — file could not be opened or read |
| 4 | BUN_ERR_USAGE — wrong number of command-line arguments |
| 5 | BUN_ERR_NOMEM — memory allocation failed |

Our implementation stores validation messages in the parse context and prints them from main.c


## 2. Decisions and Assumptions
We assume that certain features are out of scope for this implementation: zlib compression, checksum validation, and unknown compression or flag values are treated as unsupported and reported accordingly. We also assume that zero-asset files (`asset_count == 0`) are valid. Additionally, section ordering within the file is not fixed, so all validation is based purely on offsets and bounds rather than physical layout.

We address issues relating to integer overflow and wraparound when working with offsets and sizes. Since the BUN format uses 64-bit values, all arithmetic involving these fields is carefully guarded. For example, overflow is checked before performing addition:

```c
if (data_size > UINT64_MAX - data_offset)
```
This prevents overflow in the expression `data_offset + data_size`. Similarly, absolute offsets (e.g. `data_section_offset + data_offset`) are validated by checking for wraparound, which is detected when the computed result becomes smaller than the base offset due to unsigned integer overflow. This prevents malformed or malicious files from causing undefined behaviour or out-of-bounds access.

Additionally, certain redundant or impossible checks were removed during development. For example, the following checks were originally included to guard against allocation overflow:

```c
if (ctx->record_count > SIZE_MAX / sizeof(BunAssetRecord))
if (ctx->record_count > SIZE_MAX / BUN_ASSET_RECORD_SIZE)
```
However, record_count is derived from asset_count, which is a 32-bit unsigned integer (u32). Since SIZE_MAX on a 64-bit system is significantly larger than any possible u32 value, these conditions will always evaluate to false. This was confirmed by compiler warnings:
```text
warning: comparison is always false due to limited range of data type
```
As a result, these checks were removed because they do not provide meaningful safety and only add unnecessary complexity.

Instead, memory safety is enforced through an explicit sanity limit on asset_count (e.g. a maximum of 1,000,000 records). This prevents excessive memory allocation and reduces the risk of denial-of-service behaviour while keeping the implementation simple and correct.

The parser follows a “validate everything, but continue where possible” approach. Each record is validated independently using validate_record, and violations are accumulated using bun_add_violation. Rather than stopping at the first error, the parser attempts to process all records and returns a final result based on severity:
	- BUN_MALFORMED takes priority over BUN_UNSUPPORTED
	- BUN_UNSUPPORTED takes priority over BUN_OK
This ensures that the caller receives as much diagnostic information as possible.



## 3. Libraries Used

## 4. Tools Used

## 5. Security Aspects

## 6. Coding Standards

## 7. Challenges