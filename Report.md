# Phase 1 – BUN Parser Report

## Group Informaton

### Group Number: 20 

### Group Members

1. **Name: Zawad Huda**  
   **Student Number: 23102177**  
   **GitHub Username:zawadhuda**  

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
| 2 | BUN_UNSUPPORTED— file uses an unsupported feature, such as zlib compression, unsupported version, non-zero checksum, or unsupported flag bits |
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

Memory safety is enforced through an explicit sanity limit on asset_count (e.g. a maximum of 1,000,000 records). This prevents excessive memory allocation and reduces the risk of denial-of-service behaviour while keeping the implementation simple and correct.

To handle file positioning safely across platforms, a `safe_fseeko` helper function is used instead of direct `fseek` calls when working with 64-bit offsets. This prevents truncation of large file offsets on systems where `long` is 32-bit and ensures consistent behaviour when seeking within large files.

For header parsing, we decode the 60-byte BUN header manually from a byte buffer using little-endian helper functions rather than casting the raw bytes directly into a BunHeader struct. This avoids relying on compiler struct padding, alignment, or host endianness.

We treat files smaller than BUN_HEADER_SIZE as malformed, because there is not enough data to contain a valid header. However, if the header cannot be read after the file has already passed the size check, this is treated as an I/O error rather than a format violation.

The parser only marks ctx->header_parsed after all header checks pass with BUN_OK. This prevents later asset parsing from running when the section offsets, sizes, or layout are unsafe.

Header validation checks alignment for the asset table offset, string table offset, string table size, data section offset, and data section size. These fields must be 4-byte aligned, so any misalignment is treated as BUN_MALFORMED.

The reserved header field is parsed and displayed, but it is not currently treated as an error. We assume it is informational or reserved for future use unless the specification requires it to be zero.

The asset table size is calculated from asset_count * BUN_ASSET_RECORD_SIZE. Since asset_count is a 32-bit value and the record size is fixed at 48 bytes, this multiplication cannot overflow a 64-bit integer. The later addition of asset_table_offset + asset_table_size is still checked for overflow before use. The header parser allows zero-asset files. In that case, the asset table range has size zero, and later asset parsing simply records zero assets.

The implementation does not require the asset table, string table, and data section to appear in canonical order. Instead, each section is validated using its offset and size, and section overlap checks are used to reject unsafe layouts. Section overlap checks are only meaningful after the section end offsets have been calculated safely. Therefore, if any section range overflows, the parser avoids relying on those computed end offsets for overlap validation.

The parser follows a “validate everything, but continue where possible” approach. Each record is validated independently using validate_record, and violations are accumulated using bun_add_violation. Rather than stopping at the first error, the parser attempts to process all records and returns a final result based on severity:
   - `BUN_MALFORMED` takes priority over `BUN_UNSUPPORTED`
   - `BUN_UNSUPPORTED` takes priority over `BUN_OK`
This ensures that the caller receives as much diagnostic information as possible. Fatal errors such as I/O failure or memory allocation failure stop parsing immediately because continuing would be unsafe or unreliable.

[Validate_RLE]

For RLE-compressed assets, we chose to validate the compressed stream without a premature full decompression into memory. This design is implemented in `bun_validate_rle()`, which uses a fixed stack buffer:

u8 buf[4096];

and processes the payload incrementally using:

while (remaining > 0) { ... fread(...) ... }

Rather than creating an output buffer, the function only tracks the expanded size numerically:

u64 actual_uncompressed = 0;
actual_uncompressed += count;

The final total is then checked against the given metadata:

if (actual_uncompressed != r->uncompressed_size)

This keeps memory usage bounded and helps prevent decompression-bomb style behaviour.

## 3. Libraries Used

The implementation uses only standard C and POSIX-compatible libraries.

- stdio.h – file operations such as fopen, fread, fclose, fprintf
- stdlib.h – dynamic memory allocation (malloc, realloc, free)
- string.h – memory operations such as memset
- stdint.h – fixed-width integer types (uint32_t, uint64_t)
- stddef.h – size_t
- stdarg.h – variadic functions for bun_add_violation
- limits.h – numeric limits such as LLONG_MAX
- sys/types.h – off_t for file offsets

POSIX functions fseeko() and ftello() were used to safely support large file offsets.

## 4. Tools Used
[Validate_RLE]
Compiler warnings were used throughout development of the project. It was also compiled with:

gcc -std=c11 -Wall -Wextra -Wpedantic

These warnings exposed critical issues during implementation, including:

1. Function signature mismatch:

bun_parse_header(BunParseContext *ctx);

vs.

bun_parse_header(BunParseContext *ctx, BunHeader *header);

This caused compilation failure and was fixed by making the declaration and definition uniform.

2. Signed/unsigned comparison warnings when validating offsets against `ctx->file_size`.

3. Comparisons involving `SIZE_MAX` and `u32` values, which were removed after review.

The supplied unit tests were run using:

make test

These tests identified missing fixture files (`BUN_ERR_IO`), incorrect header parsing return values, and regression issues after code changes.

##GCOV

gcov was utilised to measure code coverage of the parser. The test suite was compiled with:

-fprofile-arcs -ftest-coverage

and executed using the sample unit tests

Coverage was generated using:

gcov tests/test_runner-bun_parse.gcno

The coverage report for bun_parse.c showed:

Lines executed: 50.73% of 550

This indicates that the main parsing logic, including header validation, asset parsing, and RLE validation, was exercised during testing. Both valid and invalid RLE cases were covered, including zero-count runs, truncated data, and mismatched sizes.

Some branches remain uncovered, primarily defensive checks and rare error paths that are not triggered by the provided test cases.
To increase the covered percentage to acceptable standards, we created custom tests to test the remainder of the code. 


### GCC Warning Flags

We used GCC warning flags during compilation to detect common C programming mistakes. The project was compiled with flags including '-std=c11', '-Wall', '-Wextra', and '-Wpedantic'.

Command used:

```bash
make test-asan
```

- First finding:

GCC reported signedness comparison warnings in 'bun_parse_header', where calculated u64 section end offsets were compared with ctx->file_size.

```
bun_parse.c:490:17: warning: comparison of integer expressions of different signedness: ‘u64’ {aka ‘long unsigned int’} and ‘long int’ [-Wsign-compare]
490 | if (asset_end > ctx->file_size){
| ^
bun_parse.c:511:24: warning: comparison of integer expressions of different signedness: ‘u64’ {aka ‘long unsigned int’} and ‘long int’ [-Wsign-compare]
511 | if (string_table_end > ctx->file_size){
| ^
bun_parse.c:532:24: warning: comparison of integer expressions of different signedness: ‘u64’ {aka ‘long unsigned int’} and ‘long int’ [-Wsign-compare]
532 | if (data_section_end > ctx->file_size){
| ^
```

These warnings were relevant because the parser performs bounds checks using file offsets and section sizes from an input file. Signed and unsigned comparisons can hide logic errors in bounds checks, especially when offsets and sizes may be attacker-controlled.

Change made:

We fixed the warnings by converting ctx->file_size to u64 before comparing it with calculated section end values. This made the bounds checks use consistent unsigned integer types.

Evidence:

GitHub issue: #9, Compiler warning: signedness comparison in header bounds checks

- Second finding:

GCC also reported that an asset allocation overflow check in 'bun_parse_assets' was ineffective because the condition was always false due to the limited range of 'ctx->record_count'.

This was relevant because 'asset_count' comes from the input file and is used to allocate the asset record array. If this value is extremely large, the parser could attempt an unreasonable allocation.

Change made:

We removed the unreachable overflow check and replaced it with a practical sanity limit. If 'asset_count' is greater than 1,000,000, the parser records a violation and returns 'BUN_MALFORMED' before allocating the records array.

Evidence:

GitHub issue: #10, Compiler warning: unreachable overflow check in asset allocation

Final result:

After the fix, the issue was closed as completed and the test suite passed with all 22 tests passing and no failures or errors.

### cppcheck

We used cppcheck as a static analysis tool to detect possible memory-management and code-quality issues.

Command used:

```bash
make lint
```

Finding:
cppcheck reported a memory leak in bun_parse.c during asset name validation:

```
Checking bun_parse.c ...
bun_parse.c:244:11: error: Memory leak: name_buf [memleak]
return BUN_ERR_NOMEM;
^
```

This was important because name_buf is dynamically allocated while validating asset names. If an error path returned before freeing it, malformed files could cause the parser to leak memory.

Change made:

We added free(name_buf) before returning from the affected error paths. This ensures that the temporary asset-name buffer is released even when bun_add_violation fails.

Evidence:

GitHub issue: #18, Fix cppcheck memory leak in asset name validation

Final result:

After the fix, the cppcheck memory leak warning was resolved.

## 5. Security Aspects

## 6. Coding Standards
We adopted a consistent C coding style during implementation of the BUN parser to ensure readability and maintainability. All functions and variables use snake_case naming, while struct types follow PascalCase as defined in bun.h. We avoid mixing naming conventions to keep the codebase consistent across multiple contributors.

We use early returns for error handling to simplify control flow and avoid deeply nested conditional logic. All validation functions return a bun_result_t to ensure consistent error propagation across parsing stages. Any detected violation is recorded using bun_add_violation() while still allowing the function to terminate appropriately depending on severity.

All multi-byte fields read from the binary file are converted using little-endian helper functions, ensuring portability across different architectures. Arithmetic involving offsets and lengths is consistently performed using u64 casting to prevent integer overflow during boundary checks.

## 7. Challenges
The main logistical challenge we faced during this phase was the relatively short development deadline, which required good coordination and prioritisation of tasks. To manage this, we combined both in-person and online meetings to accommodate different schedules and ensure consistent communication and progress across the group. We also maintained good communication through MS Teams and used GitHub efficiently for version control, pull requests, and collaboration. This workflow helped us resolve issues more quickly, stay aligned on design decisions, and integrate each other’s work more smoothly despite the time constraints.

### bun_parse_header function

For header parsing, the main challange was the number of layout and validity rules the BUN spec expects us to enforce without turning the function into a wall of if statements. The header has several offsets and sizes, and we have to check alignment, file bounds, and non-overlap between sections. The hard part is keeping those checks complete and readable at the same time, so we leaned on careful ordering of validations and lots of edge-case testing.

Another challange was 64-bit range arithmetic. Fields like data_section_offset and string_table_size are u64, so we treat offset + size as unsafe until we have proven it cannot overflow. That means we compute section ends with explicit overflow guards and only use the resulting ranges for overlap and bounds checks if those computations are valid.

### bun_print_summary function

For bun_print_summary, the main constraint is that names and payloads can be arbitrarily large, so we cannot safely read whole fields just to print them. The function prints a fixed 60-byte preview per field using fixed-size buffers, and it clamps the read length when the available name/payload is smaller than the preview. Reads are also bounded to the relevant section ranges so we do not accidentally step past the end of the string table or data section.

Output formatting depends on the bytes we see. If the preview is mostly printable, we print it as text. Otherwise we fall back to a hex dump so binary data does not turn into garbage output or control characters in the terminal.

File positioning is the other place this can go wrong. The function uses safe_fseeko and fread to seek into the string table and data section based on offsets from the parsed header and asset records, so the derived offsets have to be checked and kept consistent. If any seek or read fails, we treat it as an I/O failure rather than continuing with a bad file position.


