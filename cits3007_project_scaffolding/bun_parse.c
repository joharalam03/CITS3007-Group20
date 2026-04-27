#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stint.h>

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

static u16 read_u16_le(const u8 *buf, size_t offset) {
  return (u16)buf[offset]
     | (u16)buf[offset + 1] << 8;
}

static u64 read_u64_le(const u8 *buf, size_t offset) {
  return (u64)buf[offset]
     | (u64)buf[offset + 1] << 8
     | (u64)buf[offset + 2] << 16
     | (u64)buf[offset + 3] << 24 
     | (u64)buf[offset + 4] << 32
     | (u64)buf[offset + 5] << 40
     | (u64)buf[offset + 6] << 48
     | (u64)buf[offset + 7] << 56;
}


static int bun_add_violation(BunParseContext *ctx, const char *fmt, ...){
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

static void bun_ctx_free(BunParseContext *ctx) {
  // Free mem0ry
    free(ctx->records);
    free(ctx->violations);
    ctx->records            = NULL;
    ctx->violations         = NULL;
    ctx->record_count       = 0;
    ctx->violation_count    = 0;
    ctx->violation_capacity = 0;
}

static void bun_print_summary(const BunParseContext *ctx, FILE *out) {
    (void)ctx;
    fprintf(out, "(bun_print_summary not yet implemented)\n");
}


static bun_result_t bun_validate_rle(BunParseContext *ctx, u32 i, const BunAssetRecord *r) {
  // data_size needs to be even since each RLE pair is 2 bytes
  if ((r->data_size % 2u) != 0u) {
      bun_add_violation(ctx, "asset %u: RLE data_size %llu is not even", i, (unsigned long long)r->data_size);
      return BUN_MALFORMED;
  }

  u64 abs_data_offset = ctx->header.data_section_offset + r->data_offset;

  // overflow check
  if (abs_data_offset < ctx->header.data_section_offset) {
      bun_add_violation(ctx, "asset %u: data offset overflow", i);
      return BUN_MALFORMED;
  }

  if (fseek(ctx->file, (long)abs_data_offset, SEEK_SET) != 0) {
      return BUN_ERR_IO;
  }

  // read in small chunks so we dont load the whole thing into memory
  u8 buf[4096];
  u64 remaining = r->data_size;
  u64 actual_uncompressed = 0;

  while (remaining > 0) {
      size_t chunk_size = sizeof(buf);
      if (remaining < (u64)chunk_size)
          chunk_size = (size_t)remaining;

      size_t got = fread(buf, 1, chunk_size, ctx->file);
      if (got != chunk_size) {
          bun_add_violation(ctx, "asset %u: truncated RLE data", i);
          return BUN_MALFORMED;
      }

      for (size_t j = 0; j < chunk_size; j += 2) {
          u8 count = buf[j];
          // count of 0 doesnt make sense
          if (count == 0) {
              bun_add_violation(ctx, "asset %u: zero count in RLE pair", i);
              return BUN_MALFORMED;
          }

          // make sure we dont overflow when accumulating
          if (actual_uncompressed > UINT64_MAX - (u64)count) {
              bun_add_violation(ctx, "asset %u: uncompressed size overflow", i);
              return BUN_MALFORMED;
          }
          actual_uncompressed += count;
      }

      remaining -= (u64)chunk_size;
  }

  // final check - does the total match what the header said
  if (actual_uncompressed != r->uncompressed_size) {
      bun_add_violation(ctx, "asset %u: RLE uncompressed size mismatch (got %llu, expected %llu)",
          i,
          (unsigned long long)actual_uncompressed,
          (unsigned long long)r->uncompressed_size);
      return BUN_MALFORMED;
  }

  return BUN_OK;
}


/**
 * Decode a raw 48-byte buffer into a BunAssetRecord.
 */
static void parse_record(BunAssetRecord *r, const u8 *buf) {   // CHANGED (new function)
  r->name_offset = read_u32_le(buf, 0);
  r->name_length = read_u32_le(buf, 4);

  r->data_offset = read_u64_le(buf, 8);
  r->data_size   = read_u64_le(buf, 16);

  r->uncompressed_size = read_u64_le(buf, 24);

  r->compression = read_u32_le(buf, 32);
  r->type        = read_u32_le(buf, 36);

  r->checksum = read_u32_le(buf, 40);
  r->flags    = read_u32_le(buf, 44);
}

/**
 * Validate a single asset record against the BUN specification.
 * Accumulates violations.
 */
static bun_result_t validate_record(BunParseContext *ctx, u32 i, const BunAssetRecord *r) {
  bun_result_t result = BUN_OK;

  // VALIDATE NAME
  u64 name_offset = r->name_offset;
  u64 name_length = r->name_length;

  int name_valid = 1;

  if (name_length > UINT64_MAX - name_offset) {
      if (bun_add_violation(ctx, "asset %u: name range overflow", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result =  BUN_MALFORMED;
      name_valid = 0;
  }
  u64 name_end = name_offset + name_length;

  if (r->name_length < 1 || name_end > ctx->header.string_table_size) {
      if (bun_add_violation(ctx, "asset %u: invalid name bounds", i) != 0 ) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
      name_valid = 0;
  }

  u64 abs_name_offset = ctx->header.string_table_offset + name_offset;
  if (abs_name_offset < ctx->header.string_table_offset) {
      if (bun_add_violation(ctx, "asset %u: absolute name offset overflow", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
      name_valid = 0;
  }

  if (name_length > UINT64_MAX - abs_name_offset || abs_name_offset + name_length > (u64)ctx->file_size) {
    if (bun_add_violation(ctx, "asset %u: name exceeds file bounds", i) != 0) {
      return BUN_ERR_NOMEM;
    }
    name_valid = 0;
    result = BUN_MALFORMED;
  }

  if (name_valid) {
    if (fseek(ctx->file, (long)abs_name_offset, SEEK_SET) != 0) {
        return BUN_ERR_IO; 
    }

    char *name_buf = malloc((size_t)r->name_length);
    if (!name_buf) {
        return BUN_ERR_NOMEM; 
    }

    if (fread(name_buf, 1, (size_t)r->name_length, ctx->file) != (size_t)r->name_length) {
        if (bun_add_violation(ctx, "asset %u: name read error", i) != 0) {
          return BUN_ERR_NOMEM;
        }
        free(name_buf);
        return BUN_MALFORMED; 
    }

    for (u32 j = 0; j < r->name_length; j++) {
        if ((unsigned char)name_buf[j] < 0x20 ||
            (unsigned char)name_buf[j] > 0x7E) {

            if (bun_add_violation(ctx, "asset %u: non-printable character in name", i) != 0) {
              return BUN_ERR_NOMEM;
            }

            result = BUN_MALFORMED;
            break; 
        }
    }

    free(name_buf);
  }
  // VALIDATE DATA
  u64 data_offset = r->data_offset;         
  u64 data_size = r->data_size;              
  if (data_size > UINT64_MAX - data_offset) {
      if (bun_add_violation(ctx, "asset %u: data range overflow", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
  }
  u64 data_end = data_offset + data_size;

  if (data_end > ctx->header.data_section_size) {
      if (bun_add_violation(ctx, "asset %u: data out of bounds", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
  }

  u64 abs_data_offset = ctx->header.data_section_offset + data_offset;
  if (abs_data_offset < ctx->header.data_section_offset) {
      if (bun_add_violation(ctx, "asset %u: absolute data offset overflow", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
  }
  if (data_size > UINT64_MAX - abs_data_offset ||
      abs_data_offset + data_size > (u64)ctx->file_size) {

      if (bun_add_violation(ctx, "asset %u: data exceeds file size", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
  }

  if (r->compression == 2) {
      if (bun_add_violation(ctx, "asset %u: zlib compression not supported", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      if (result != BUN_MALFORMED) {
          result = BUN_UNSUPPORTED;
      }
  }

  if (r->compression > 2) {
      if (bun_add_violation(ctx, "asset %u: invalid compression type", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      if (result != BUN_MALFORMED) {
          result = BUN_UNSUPPORTED;
      }
  }

  if (r->compression == 0 && r->uncompressed_size != 0) {
      if (bun_add_violation(ctx, "asset %u: invalid uncompressed size", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      result = BUN_MALFORMED;
  }

  if (r->checksum != 0) {
      if (bun_add_violation(ctx, "asset %u: checksum not supported", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      if (result != BUN_MALFORMED) {
          result = BUN_UNSUPPORTED;
      }
  }

  u32 allowed = BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE;

  if (r->flags & ~allowed) {
      if (bun_add_violation(ctx, "asset %u: unknown flag bits set", i) != 0) {
        return BUN_ERR_NOMEM;
      }
      if (result != BUN_MALFORMED) {
          result = BUN_UNSUPPORTED;
      }
  }

  if (r->compression == 1) {
    bun_result_t rle_res = bun_validate_rle(ctx, i, r);

    if (rle_res == BUN_MALFORMED) {
        result = BUN_MALFORMED;
    } else if (rle_res == BUN_UNSUPPORTED) {
        if (result != BUN_MALFORMED) {
            result = BUN_UNSUPPORTED;
        }
    }
  }

  return result;
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
    bun_add_violation(ctx, "File length was too small");
    return BUN_MALFORMED;
  }

  // slurp the header into `buf`
  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    return BUN_ERR_IO;
  }

  BunHeader *h = &ctx->header;
  

  h->magic = read_u32_le(buf, 0);
  h->version_major = read_u16_le(buf, 4);
  h->version_minor = read_u16_le(buf, 6);
  h->asset_count = read_u32_le(buf, 8);
  h->asset_table_offset = read_u64_le(buf, 12); 
  h->string_table_offset = read_u64_le(buf, 20);
  h->string_table_size = read_u64_le(buf, 28);
  h->data_section_offset = read_u64_le(buf, 36);
  h->data_section_size = read_u64_le(buf, 44);
  h->reserved = read_u64_le(buf, 52);

  if (h->magic != BUN_MAGIC) {
    bun_add_violation(ctx, "invalid magic value");
    return BUN_MALFORMED;
  }

  if (h->version_major != 1 || h->version_minor != 0){
    bun_add_violation(ctx, "Version %d.%d unsupported", h->version_major, h->version_minor);
    return BUN_UNSUPPORTED;
  }

  if ((h->asset_table_offset & 3) != 0){
    bun_add_violation(ctx, "Asset table offset not aligned");
    return BUN_MALFORMED;
  }

  if ((h->string_table_offset & 3) != 0){
    bun_add_violation(ctx, "String table offset not aligned");
    return BUN_MALFORMED;
  }

  if ((h->string_table_size & 3) != 0){
    bun_add_violation(ctx, "String table size not aligned");
    return BUN_MALFORMED;
  }

  if ((h->data_section_offset & 3) != 0){
    bun_add_violation(ctx, "Data section offset not aligned");
    return BUN_MALFORMED;
  }

  if ((h->data_section_size & 3) != 0){
    bun_add_violation(ctx, "Data section size not aligned");
    return BUN_MALFORMED;
  }

  u64 asset_table_size = (u64)h->asset_count * BUN_ASSET_RECORD_SIZE;
  u64 asset_start = h->asset_table_offset;
  u64 asset_end = asset_start + asset_table_size;

  if (asset_end < asset_start){
    bun_add_violation(ctx, "Asset table overflow");
    return BUN_MALFORMED;
  }

  if (asset_end > (u64) ctx->file_size){
    bun_add_violation(ctx, "Asset table exceeds file size");
    return BUN_MALFORMED;
  }

  u64 string_table_start = h->string_table_offset;
  u64 string_table_end = string_table_start + h->string_table_size;

  if (string_table_end < string_table_start){
    bun_add_violation(ctx, "String table overflow");
    return BUN_MALFORMED;
  }

  if (string_table_end > (u64) ctx->file_size){
    bun_add_violation(ctx, "String table exceeds file size");
    return BUN_MALFORMED;
  }

  u64 data_section_start = h->data_section_offset;
  u64 data_section_end = data_section_start + h->data_section_size;

  if (data_section_end < data_section_start){
    bun_add_violation(ctx, "Data section overflow");
    return BUN_MALFORMED;
  }

  if (data_section_end > (u64) ctx->file_size){
    bun_add_violation(ctx, "Data section exceeds file size");
    return BUN_MALFORMED;
  }

  if (!(asset_end <= string_table_start || string_table_end <= asset_start)){
    bun_add_violation(ctx, "Asset and String overlap");
    return BUN_MALFORMED;
  }

   if (!(string_table_end <= data_section_start || data_section_end <= string_table_start)){
    bun_add_violation(ctx, "String and Data overlap");
    return BUN_MALFORMED;
  }

   if (!(asset_end <= data_section_start || data_section_end <= asset_start)){
    bun_add_violation(ctx, "Asset and Data overlap");
    return BUN_MALFORMED;
  }

  ctx->header_parsed = 1;   
  return BUN_OK;
}

bun_result_t bun_parse_assets(BunParseContext *ctx) {
  /**
 * Parses all asset records from the asset table and validates each entry.
 * Accumulates violations.
 */
  if (!ctx->header_parsed) {
      return BUN_ERR_USAGE;
    }

  // move file pointer to start of asset table
  if (fseek(ctx->file, ctx->header.asset_table_offset, SEEK_SET) != 0) {
    return BUN_ERR_IO;
  }

  // zero-check (safe edge case handling)
  if (ctx->header.asset_count == 0) {
    ctx->records = NULL;
    ctx->record_count = 0;
    return BUN_OK;
  }

  // allocate array for all asset records
  ctx->record_count = ctx->header.asset_count;
  if (ctx->record_count > 1000000) {
    if (bun_add_violation(ctx, "asset_count too large: %u", ctx->record_count) != 0) {
      return BUN_ERR_NOMEM;
    }
    return BUN_MALFORMED;
  }

  if (ctx->record_count > SIZE_MAX / sizeof(BunAssetRecord)) {
    return BUN_ERR_NOMEM;
  }

  ctx->records = malloc(sizeof(BunAssetRecord) * ctx->record_count);
  if (!ctx->records) {
    return BUN_ERR_NOMEM;
  }

  bun_result_t final_result = BUN_OK;

    // parse and validate all records
  for (u32 i = 0; i < ctx->record_count; i++) {
    u8 buf[BUN_ASSET_RECORD_SIZE];

    u64 record_offset = ctx->header.asset_table_offset + (u64)i * BUN_ASSET_RECORD_SIZE;

    if (fseek(ctx->file, (long)record_offset, SEEK_SET) != 0) {
      free(ctx->records);
      ctx->records = NULL;
      ctx->record_count = 0;
      return BUN_ERR_IO;
    }

    size_t n = fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file);
    if (n != BUN_ASSET_RECORD_SIZE) {
      if (bun_add_violation(ctx, "asset %u: incomplete record", i) != 0) {
        free(ctx->records);
        ctx->records = NULL;
        ctx->record_count = 0;
        return BUN_ERR_NOMEM;
      }

      free(ctx->records);
      ctx->records = NULL;
      ctx->record_count = 0;

      return BUN_MALFORMED;
    }

    BunAssetRecord *r = &ctx->records[i];
    memset(r, 0, sizeof(*r));
    parse_record(r, buf);

    // validate record against specification rules
    bun_result_t res = validate_record(ctx, i, r);
    if (res == BUN_ERR_IO || res == BUN_ERR_NOMEM) {
      free(ctx->records);
      ctx->records = NULL;
      ctx->record_count = 0;
      return res;
    }
    if (res == BUN_MALFORMED) {
      final_result = BUN_MALFORMED;  
    } else if (res == BUN_UNSUPPORTED) {
      if (final_result != BUN_MALFORMED) {
        final_result = BUN_UNSUPPORTED;
      }
    }
  }
  return final_result;
}

void bun_print_summary(const BunParseContext *ctx, FILE *out)
{
    if (ctx == NULL || out == NULL) {
        return;
    }

    const BunHeader *h = &ctx->header;

    fprintf(out, "BUN header\n");
    fprintf(out, "magic               = 0x%08x\n", h->magic);
    fprintf(out, "version_major       = %u\n", h->version_major);
    fprintf(out, "version_minor       = %u\n", h->version_minor);
    fprintf(out, "asset_count         = %u\n", h->asset_count);
    fprintf(out, "asset_table_offset  = %llu\n", (unsigned long long)h->asset_table_offset);
    fprintf(out, "string_table_offset = %llu\n", (unsigned long long)h->string_table_offset);
    fprintf(out, "string_table_size   = %llu\n", (unsigned long long)h->string_table_size);
    fprintf(out, "data_section_offset = %llu\n", (unsigned long long)h->data_section_offset);
    fprintf(out, "data_section_size   = %llu\n", (unsigned long long)h->data_section_size);
    fprintf(out, "reserved            = %llu\n", (unsigned long long)h->reserved);

    fprintf(out, "\nAsset records\n");

    for (u32 i = 0; i < ctx->record_count; i++) {
        const BunAssetRecord *r = &ctx->records[i];

        fprintf(out, "\nAsset %u\n", i);
        fprintf(out, "name_offset       = %u\n", r->name_offset);
        fprintf(out, "name_length       = %u\n", r->name_length);
        fprintf(out, "data_offset       = %llu\n", (unsigned long long)r->data_offset);
        fprintf(out, "data_size         = %llu\n", (unsigned long long)r->data_size);
        fprintf(out, "uncompressed_size = %llu\n", (unsigned long long)r->uncompressed_size);
        fprintf(out, "compression       = %u\n", r->compression);
        fprintf(out, "type              = %u\n", r->type);
        fprintf(out, "checksum          = %u\n", r->checksum);
        fprintf(out, "flags             = 0x%08x\n", r->flags);


        size_t name_len = r->name_length;

        if (name_len > 60) {
            name_len = 60;
        }

        fprintf(out, "name_preview = ");

        if (name_len == 0) {
            fprintf(out, "empty");
        } else {
            u8 name_buf[60];

            u64 name_pos = h->string_table_offset + (u64)r->name_offset;

            if (fseek(ctx->file, (long)name_pos, SEEK_SET) == 0 &&
                fread(name_buf, 1, name_len, ctx->file) == name_len) {

                int printable = 1;

                for (size_t j = 0; j < name_len; j++) {
                    if (name_buf[j] < 0x20 || name_buf[j] > 0x7e) {
                        printable = 0;
                        break;
                    }
                }

                if (printable) {
                    for (size_t j = 0; j < name_len; j++) {
                        fputc(name_buf[j], out);
                    }
                } else {
                    fprintf(out, "hex:");
                    for (size_t j = 0; j < name_len; j++) {
                        fprintf(out, " %02x", name_buf[j]);
                    }
                }
            } else {
                fprintf(out, "could not read name");
            }
        }

        fprintf(out, "\n");

        size_t data_len = 60;

        if (r->data_size < (u64)data_len) {
            data_len = (size_t)r->data_size;
        }

        fprintf(out, "data_preview = ");

        if (data_len == 0) {
            fprintf(out, "empty");
        } else {
            u8 data_buf[60];

            u64 data_pos = h->data_section_offset + r->data_offset;

            if (fseek(ctx->file, (long)data_pos, SEEK_SET) == 0 &&
                fread(data_buf, 1, data_len, ctx->file) == data_len) {

                int printable = 1;

                for (size_t j = 0; j < data_len; j++) {
                    if (data_buf[j] < 0x20 || data_buf[j] > 0x7e) {
                        printable = 0;
                        break;
                    }
                }

                if (printable) {
                    for (size_t j = 0; j < data_len; j++) {
                        fputc(data_buf[j], out);
                    }
                } else {
                    fprintf(out, "hex:");
                    for (size_t j = 0; j < data_len; j++) {
                        fprintf(out, " %02x", data_buf[j]);
                    }
                }
            } else {
                fprintf(out, "could not read data");
            }
        }

        fprintf(out, "\n");
    }
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