#include <stdio.h>
#include <stdlib.h>

#include "bun.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
    return BUN_ERR_USAGE;
  }
  const char *path = argv[1];

  BunParseContext ctx = {0};


  bun_result_t open_r = bun_open(path, &ctx);
  if (open_r != BUN_OK) {
    fprintf(stderr, "Error: could not open '%s'\n", path);
    bun_ctx_free(&ctx);
    return open_r;
  }


  bun_result_t hdr_r = bun_parse_header(&ctx);
  
  // Parse assets when header parsed
  bun_result_t assets_r = BUN_OK;
  if (ctx.header_parsed) {
    assets_r = bun_parse_assets(&ctx);
  }

  bun_result_t final;
  // Add checks to find any possible violations
  if (hdr_r == BUN_UNSUPPORTED || assets_r == BUN_UNSUPPORTED) {
    final = BUN_UNSUPPORTED;
  } else if (hdr_r == BUN_MALFORMED || assets_r == BUN_MALFORMED) {
    final = BUN_MALFORMED;
  } else if (hdr_r == BUN_ERR_IO || assets_r == BUN_ERR_IO) {
    final = BUN_ERR_IO;
  } else if (hdr_r == BUN_ERR_NOMEM || assets_r == BUN_ERR_NOMEM) {
    final = BUN_ERR_NOMEM;
  } else {
    final = BUN_OK;
  }

  // Print the violations found
  for (size_t i = 0; i < ctx.violation_count; i++) {
    fprintf(stderr, "Violation: %s\n", ctx.violations[i].message);
  }

  if (ctx.header_parsed) {
    bun_print_summary(&ctx, stdout);
  }

  bun_close(&ctx);
  bun_ctx_free(&ctx);
  return final;
}
