# Test log - Page Fault Analyzer

All results below were produced by the compiled program (`gcc -std=c99 -Wall -Wextra -pedantic -O2`),
by replaying the `input_*.txt` files (`./page_fault_analyzer < tests/input_XX.txt`) and by
`./page_fault_analyzer --selftest`. The matching outputs are in `output_*.txt`.

| # | Input / action | Expected behaviour | Actual result | Status |
|---|---|---|---|---|
| T1 | `--selftest` | All checks pass, exit code 0 | 15 passed, 0 failed | Pass |
| T2 | Textbook string `7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1`, 3 frames (input_01) | FIFO 15, LRU 12, Optimal 9 faults (standard textbook values) | 15 / 12 / 9 | Pass |
| T3 | Belady string `1 2 3 4 1 2 5 1 2 3 4 5`, 3 and 4 frames (input_02, self-test) | FIFO 9 -> 10 faults (Belady's anomaly); LRU 10 -> 8; Optimal 7 -> 6 | 9->10, 10->8, 7->6; anomaly reported for FIFO | Pass |
| T4 | Single reference `9`, 1 frame (input_06) | 1 fault for each algorithm | 1 / 1 / 1 | Pass |
| T5 | `1 2 1 2 1 2`, 1 frame (input_06) | Every reference faults | 6 / 6 / 6 | Pass |
| T6 | `3 3 3 3`, 2 frames (self-test) | 1 fault, 3 hits | 1 / 1 / 1 | Pass |
| T7 | `1 2 3 1 2 3`, 5 frames (more frames than pages) (self-test) | Only compulsory faults (3) | 3 / 3 / 3 | Pass |
| T8 | Each of 5 workloads x frames 1..20 x seeds 1, 42, 2024 (self-test) | hits + faults = references; faults >= distinct pages | Pass | Pass |
| T9 | Same grid as T8 | Optimal faults <= FIFO and <= LRU | Pass | Pass |
| T10 | Same seed generated twice (self-test); same input session run twice and compared with `cmp` | Identical reference strings / identical output | Identical | Pass |
| T11 | Seed 99 versus seed 100, random workload, 50 references (input_09, input_10) | Different reference strings | Outputs differ | Pass |
| T12 | Menu input `abc`, `99`, `-1` (input_05) | Rejected with a message, menu shown again | "please enter a whole number" / "value must be between 0 and 10" | Pass |
| T13 | Sequence `1 2 x`, `1 2 200`, `1 -3` (input_05) | Each rejected with the reason | Messages for non-numeric token, page 200 out of range, page -3 out of range | Pass |
| T14 | Empty sequence line (input_05) | Cancel, previous workload kept | "Cancelled - previous workload kept." | Pass |
| T15 | 201 references on one line | Rejected | "too many references (maximum is 200)" | Pass |
| T16 | Exactly 200 references, 20 frames | Accepted, no overflow | Ran all three algorithms, hits + faults = 200 | Pass |
| T17 | 5000-character input line | Rejected, rest of line discarded, no crash | "line too long (maximum 2047 characters)" | Pass |
| T18 | Frames `0` and `21` (input_05) | Rejected | "value must be between 1 and 20" | Pass |
| T19 | y/n prompt answered with a number (input_05) | Re-prompt | "Please answer y or n." | Pass |
| T20 | Input stream ends in the middle of a prompt | Clean exit, no infinite loop | "[Input closed - exiting.]", exit code 0 | Pass |
| T21 | Option 8 before any analysis | Friendly message | Handled | Pass |
| T22 | Database workload, 4 frames (input_03) | LRU is allowed to beat FIFO; the data decides | FIFO 38, LRU 33, Optimal 24 | Pass |
| T23 | Memory-pressure workload, 4 frames, seed 7 (input_08) | Result decided by data, no assumption that LRU wins | FIFO 56, LRU 58, Optimal 43 (FIFO beat LRU) | Pass |
| T24 | Sequential scan, 36 references, 3 frames (input_07) | Cyclic scan over 12 pages: FIFO and LRU fault on every reference | 36 / 36 / 30 | Pass |
| T25 | Change frames while memory-pressure workload is selected (input_07) | Workload regenerated for the new frame count | "regenerated for 5 frames (20 distinct pages)" | Pass |
| T26 | Frame experiment 1..8 on database and memory-pressure workloads | Optimal <= others at every capacity; Belady cases reported | OK reported; no anomaly on these workloads | Pass |
| T27 | All `input_*.txt` sessions and `--selftest` under `-fsanitize=address,undefined` | No sanitizer reports | None | Pass |

## Corrections made during testing

1. Step-by-step table: an empty frame was printed as ` - ` which misaligned the columns
   against occupied frames. Changed so empty frames print as `-` left-aligned like numbers.
2. Wording: `1 faults` / `1 references` / `at 1 capacities` read poorly for singular values;
   changed to `fault(s)`, `reference(s)` and `capacity(ies)`.
3. A first batch of scripted sessions failed to start because the test directory had not
   been created; this was a test-setup mistake, not a program defect.

No defect was found in the FIFO, LRU or Optimal logic during testing.
