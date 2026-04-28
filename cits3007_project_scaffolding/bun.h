#ifndef BUN_H
#define BUN_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>   // NEW: for size_t
#include <stdarg.h>
#include <stdlib.h>

//
// Result codes (per BUN spec section 2)
//

typedef enum {
    BUN_OK          = 0,
    BUN_MALFORMED   = 1, 
    BUN_UNSUPPORTED = 2,
    BUN_ERR_IO      = 3,   /* I/O error or file not found */
    BUN_ERR_USAGE   = 4,   /* wrong number of arguments */
    BUN_ERR_NOMEM   = 5,   /* memory allocation failed */
} bun_result_t;

//
// Data types (per BUN spec section 2)
// All multi-byte integers are little-endian on disk.
//

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

//
// On-disk structures (per BUN spec sections 4 and 5)
//

#define BUN_MAGIC         0x304E5542u   // "BUN0" in little-endian
#define BUN_VERSION_MAJOR 1
#define BUN_VERSION_MINOR 0

#define BUN_FLAG_ENCRYPTED  0x1u
#define BUN_FLAG_EXECUTABLE 0x2u

typedef struct {
    u32 magic;
    u16 version_major;
    u16 version_minor;
    u32 asset_count;
    u64 asset_table_offset;
    u64 string_table_offset;
    u64 string_table_size;
    u64 data_section_offset;
    u64 data_section_size;
    u64 reserved;
} BunHeader;

typedef struct {
    u32 name_offset;
    u32 name_length;
    u64 data_offset;
    u64 data_size;
    u64 uncompressed_size;
    u32 compression;
    u32 type;
    u32 checksum;
    u32 flags;
} BunAssetRecord;

//
// Expected on-disk sizes
//

#define BUN_HEADER_SIZE       60
#define BUN_ASSET_RECORD_SIZE 48

// A violation is a single error message the parser produces when it spots
// something wrong in the .bun file. The parser stores violations in the
// BunParseContext as it goes; main.c prints them to stderr at the end.
//

#define BUN_VIOLATION_MAX 256   // max length of a single violation string

typedef struct {
    char message[BUN_VIOLATION_MAX];
} BunViolation;


// A struct to store the parser's state and results. Created empty by main.c
// and passed into each parser function. Fields filled in as parsing proceeds.
//

typedef struct {
    // --- file handle (existing) ---
    FILE *file;           // open file handle
    long  file_size;      // total file size in bytes

    // --- NEW: decoded header (filled by bun_parse_header) ---
    BunHeader header;
    int       header_parsed;   // 0 = not yet, 1 = done

    // --- NEW: decoded asset records (filled by bun_parse_assets) ---
    BunAssetRecord *records;       // heap-allocated array of length record_count
    u32             record_count;

    // --- NEW: list of violations collected during parsing ---
    BunViolation *violations;          // heap-allocated growable array
    size_t        violation_count;     // number of messages currently stored
    size_t        violation_capacity;  // number of slots allocated
} BunParseContext;

//
// Public API
//
// Parser functions return result codes and do not print to stdout or stderr
// themselves. All output happens in main.c based on the returned code and
// the contents of the BunParseContext.
//

/**
 * Open a BUN file and populate ctx->file and ctx->file_size.
 * Returns BUN_ERR_IO if the file cannot be opened or its size determined.
 */
bun_result_t bun_open(const char *path, BunParseContext *ctx);

/**
 * Parse and validate the BUN header from ctx->file.
 * On BUN_OK: sets ctx->header and ctx->header_parsed = 1.
 * On BUN_MALFORMED/BUN_UNSUPPORTED: appends one or more messages to
 * ctx->violations describing the problem(s) found.
 */
bun_result_t bun_parse_header(BunParseContext *ctx);

/**
 * Parse and validate all asset records. Must be called after bun_parse_header.
 * On BUN_OK: allocates ctx->records and sets ctx->record_count.
 * On BUN_MALFORMED/BUN_UNSUPPORTED: appends messages to ctx->violations.
 */
bun_result_t bun_parse_assets(BunParseContext *ctx);

/**
 * Close the file handle in ctx. Does not free heap memory -- use
 * bun_ctx_free for that.
 */
bun_result_t bun_close(BunParseContext *ctx);

void bun_print_summary(const BunParseContext *ctx, FILE *out);

void bun_ctx_free(BunParseContext *ctx);

#endif // BUN_H