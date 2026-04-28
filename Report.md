# Phase 1 – BUN Parser Report

## Group Informaton

### Group Number: 20 

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

There are certain errors that prevent futhur parsing such as if the file is shorter thatn the 60 byte header, the parser reports the error and stops.

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

## 3. Libraries Used

The 'bun_parser' executable does not depend on any third-party libraries. It only uses the standard C library and standard system headers.

The unit test suite uses the third-party 'libcheck' framework, included through 'check.h'. This dependency is only required for building and running the test binary, not for the final 'bun_parser' executable.

## 4. Tools Used

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

 - GitHub issue: #9, Compiler warning: signedness comparison in header bounds checks
 - PR: #8, Testing and Tooling
 - Fix commit: 67f92c9, Fix signedness comparison in header bounds checks

- Second finding:

GCC also reported that an asset allocation overflow check in 'bun_parse_assets' was ineffective because the condition was always false due to the limited range of 'ctx->record_count'.

This was relevant because 'asset_count' comes from the input file and is used to allocate the asset record array. If this value is extremely large, the parser could attempt an unreasonable allocation.

Change made:

We removed the unreachable overflow check and replaced it with a practical sanity limit. If 'asset_count' is greater than 1,000,000, the parser records a violation and returns 'BUN_MALFORMED' before allocating the records array.

Evidence:

 - GitHub issue: #10, Compiler warning: unreachable overflow check in asset allocation
 - PR: #8, Testing and Tooling
 - Fix commit: 0bec5fe, Replace unreachable allocation overflow check

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

 - GitHub issue: #18, Fix cppcheck memory leak in asset name validation
 - PR: #19, Fix warning
 - Fix commit: 54d5edf, Fix cppcheck memory leak in asset name validation

Final result:

After the fix, the cppcheck memory leak warning was resolved.

## 5. Security Aspects

## 6. Coding Standards

## 7. Challenges