#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "bun.h"

/**
 * Example helper: convert 4 bytes in `buf`, positioned at `offset`,
 * into a little-endian u32.
 */
static u32 read_u32_le(const u8 *buf, size_t offset) {
  return (u32)buf[offset]
     | (u32)buf[offset + 1] << 8
     | (u32)buf[offset + 2] << 16
     | (u32)buf[offset + 3] << 24;
}

//
// API implementation
//

bun_result_t bun_open(const char *path, BunParseContext *ctx) {
  // we open the file; seek to the end, to get the size; then jump back to the
  // beginning, ready to start parsing.

  ctx->file = fopen(path, "rb");
  if (!ctx->file) {
    return BUN_ERR_IO;
  }

  if (fseek(ctx->file, 0, SEEK_END) != 0) {
    fclose(ctx->file);
    return BUN_ERR_IO;
  }
  ctx->file_size = ftell(ctx->file);
  if (ctx->file_size < 0) {
    fclose(ctx->file);
    return BUN_ERR_IO;
  }
  rewind(ctx->file);

  return BUN_OK;
}

bun_result_t bun_parse_header(BunParseContext *ctx) {
  u8 buf[BUN_HEADER_SIZE];

  // our file is far too short, and cannot be valid!
  // (query: how do we let `main` know that "file was too short"
  // was the exact problem? Where can we put details about the
  // exact validation problem that occurred?)
  if (ctx->file_size < (long)BUN_HEADER_SIZE) {
    return BUN_MALFORMED;
  }

  // slurp the header into `buf`
  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    return BUN_ERR_IO;
  }

  // TODO: populate `header` from `buf`.

  // TODO: validate fields and return BUN_MALFORMED or BUN_UNSUPPORTED
  // as required by the spec. The magic check is a good place to start.

  return BUN_OK;
}

bun_result_t bun_parse_assets(BunParseContext *ctx) {

  // TODO: implement asset record parsing and validation

  return BUN_OK;
}

bun_result_t bun_close(BunParseContext *ctx) {
  assert(ctx->file);

  int res = fclose(ctx->file);
  if (res) {
    return BUN_ERR_IO;
  } else {
    ctx->file = NULL;
    return BUN_OK;
  }
}

int bun_add_violation(BunParseContext *ctx, const char *fmt, ...){
    // If array is full then grow
    if (ctx->violation_count == ctx->violation_capacity) {
        size_t capacity;

        if (ctx->violation_capacity == 0) {
            capacity = 8;
        } else {
            capacity = ctx->violation_capacity * 2;    // Grow using array pattern
        }

        // Allocate size for violations arr
        BunViolation *new_buf = realloc (ctx->violations, capacity * sizeof(BunViolation));

        // If there is no memory
        if (new_buf == NULL) {
            return -1;
        }

        ctx->violations = new_buf;
        ctx->violation_capacity = capacity;

    }

    // Move to next empty slot
    va_list args;
    va_start(args, fmt);

    vsnprintf(ctx->violations[ctx->violation_count].message, BUN_VIOLATION_MAX, fmt, args);
    va_end(args);

    ctx->violation_count++;
    return 0;

}

void bun_ctx_free(BunParseContext *ctx) {
  // Free mem0ry
    free(ctx->records);
    free(ctx->violations);
    ctx->records            = NULL;
    ctx->violations         = NULL;
    ctx->record_count       = 0;
    ctx->violation_count    = 0;
    ctx->violation_capacity = 0;
}

void bun_print_summary(const BunParseContext *ctx, FILE *out) {
    (void)ctx;
    fprintf(out, "(bun_print_summary not yet implemented)\n");
}
