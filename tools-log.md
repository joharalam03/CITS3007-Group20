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


- Tool: make test
- Environment: CITS3007 SDE VM
- Command: make test
- Result: link-stage failure
- Findings:
  - bun_parse_header produced compiler warnings for signed/unsigned comparisons
  - bun_parse_assets produced an unused-parameter warning
  - linker failed with undefined references to bun_add_violation
- Follow-up investigation:
  - searched the project for bun_add_violation using grep
  - found declarations/calls in bun.h and bun_parse.c
  - found no function definition in any .c file
- Conclusion:
  - the current blocker is a missing implementation of bun_add_violation, not the test fixture setup
  

- Tool: cppcheck
- Environment: CITS3007 SDE VM
- Command: make lint
- Result: completed successfully
- Findings: no actionable cppcheck warnings were reported in the output for bun_parse.c, main.c, or tests/test_bun.c
- Action Taken: recorded successful static-analysis run; lint target is now available for repeated use during development
- Notes: cppcheck had to be installed in the VM before the lint target could run