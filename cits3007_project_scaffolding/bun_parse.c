#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

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


// suggested function call for bun_validate_rle
static bun_result_t bun_validate_rle(BunParseContext *ctx, u32 i, const BunAssetRecord *r);

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
 */
static bun_result_t validate_record(BunParseContext *ctx, u32 i, const BunAssetRecord *r) {
  // VALIDATE NAME
  u64 name_offset = r->name_offset;
  u64 name_length = r->name_length;
  if (name_length > UINT64_MAX - name_offset) {
      bun_add_violation(ctx, "asset %u: name offset overflow", i);
      return BUN_MALFORMED;
  }
  u64 name_end = name_offset + name_length;

  if (r->name_length < 1 ||
      name_end > ctx->header.string_table_size) {

      bun_add_violation(ctx, "asset %u: invalid name bounds", i);
      return BUN_MALFORMED;
  }

  u64 abs_name_offset = ctx->header.string_table_offset + name_offset;
  if (abs_name_offset < ctx->header.string_table_offset) {
      bun_add_violation(ctx, "asset %u: name offset overflow", i);
      return BUN_MALFORMED;
  }

  if (fseek(ctx->file, (long)abs_name_offset, SEEK_SET) != 0) { 
      return BUN_ERR_IO;
  } 

  char *name_buf = malloc((size_t)r->name_length);
  if (!name_buf) {
      return BUN_ERR_NOMEM;
  }

  if (fread(name_buf, 1, (size_t)r->name_length, ctx->file) != (size_t)r->name_length) {
      bun_add_violation(ctx, "asset %u: name read error", i);
      free(name_buf);
      return BUN_MALFORMED;
  }

  for (u32 j = 0; j < r->name_length; j++) {
      if ((unsigned char)name_buf[j] < 0x20 ||
          (unsigned char)name_buf[j] > 0x7E) {

          bun_add_violation(ctx,
              "asset %u: non-printable character in name", i);

          free(name_buf);
          return BUN_MALFORMED;
      }
  }
  free(name_buf);

  // VALIDATE DATA
  u64 data_offset = r->data_offset;         
  u64 data_size = r->data_size;              
  if (data_size > UINT64_MAX - data_offset) {
      bun_add_violation(ctx, "asset %u: data offset overflow", i);
      return BUN_MALFORMED;
  }
  u64 data_end = data_offset + data_size;

  if (data_end > ctx->header.data_section_size) {
      bun_add_violation(ctx, "asset %u: data out of bounds", i);
      return BUN_MALFORMED;
  }

  if (r->compression == 2) {
      bun_add_violation(ctx, "asset %u: zlib compression not supported", i);
      return BUN_UNSUPPORTED;
  }

  if (r->compression > 2) {
      bun_add_violation(ctx, "asset %u: invalid compression type", i);
      return BUN_UNSUPPORTED;
  }

  if (r->compression == 0 && r->uncompressed_size != 0) {
      bun_add_violation(ctx, "asset %u: invalid uncompressed size", i);
      return BUN_MALFORMED;
  }

  if (r->checksum != 0) {
      bun_add_violation(ctx, "asset %u: checksum not supported", i);
      return BUN_UNSUPPORTED;
  }

  u32 allowed = BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE;

  if (r->flags & ~allowed) {
      bun_add_violation(ctx, "asset %u: unknown flag bits set", i);
      return BUN_UNSUPPORTED;
  }

  if (r->compression == 1) {
      return bun_validate_rle(ctx, i, r);
  }

  return BUN_OK;
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

  if (asset_end > ctx->file_size){
    bun_add_violation(ctx, "Asset table exceeds file size");
    return BUN_MALFORMED;
  }

  u64 string_table_start = h->string_table_offset;
  u64 string_table_end = string_table_start + h->string_table_size;

  if (string_table_end < string_table_start){
    bun_add_violation(ctx, "String table overflow");
    return BUN_MALFORMED;
  }

  if (string_table_end > ctx->file_size){
    bun_add_violation(ctx, "String table exceeds file size");
    return BUN_MALFORMED;
  }

  u64 data_section_start = h->data_section_offset;
  u64 data_section_end = data_section_start + h->data_section_size;

  if (data_section_end < data_section_start){
    bun_add_violation(ctx, "Data section overflow");
    return BUN_MALFORMED;
  }

  if (data_section_end > ctx->file_size){
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
  if (ctx->record_count > SIZE_MAX / sizeof(BunAssetRecord)) {   
    return BUN_ERR_NOMEM;
  }
  ctx->records = malloc(sizeof(BunAssetRecord) * ctx->record_count);
  if (!ctx->records) {
    return BUN_ERR_NOMEM;
  }

  // parse and validate all records
  for (u32 i = 0; i < ctx->record_count; i++) {
    u8 buf[BUN_ASSET_RECORD_SIZE];

    // read fixed-size asset record (48 bytes)
    size_t n = fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file);
    if (n != BUN_ASSET_RECORD_SIZE) {
        bun_add_violation(ctx, "asset %u: incomplete record", i);

        // cleanup on failure to avoid memory leaks
        free(ctx->records);
        ctx->records = NULL;
        ctx->record_count = 0;

        return BUN_MALFORMED;
    }

    BunAssetRecord *r = &ctx->records[i];

    parse_record(r, buf);

    // validate record against specification rules
    bun_result_t res = validate_record(ctx, i, r);
    if (res != BUN_OK) {
      // cleanup on failure to avoid memory leaks
      free(ctx->records);
      ctx->records = NULL;
      ctx->record_count = 0;

      return res;
    }
  }
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
