## 2026-04-23

- Tool: make test
- Environment: CITS3007 SDE VM
- Command: make test
- Result: build failed at link stage
- Findings:
  - warning: signed/unsigned comparison in bun_parse_header
  - warning: unused parameter ctx in bun_parse_assets
  - error: undefined reference to bun_add_violation
- Interpretation:
  - the test suite now compiles far enough to reveal a missing implementation dependency in bun_parse.c
  - current execution of sample-based tests is blocked until bun_add_violation is implemented and linked
- Next action:
  - pull the latest group changes
  - rerun make test after bun_add_violation is available


- Change: added `test-asan` target to Makefile
- Purpose: standardise running ASan/UBSan through `make test-asan`