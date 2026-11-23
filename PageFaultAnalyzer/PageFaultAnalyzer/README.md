# Page Fault Analyzer

**Subject:** Advanced C Programming  
**Type:** Console-based page-replacement simulation and analysis tool  
**Language:** Standard C (C99). Only the C standard library is used.

Page Fault Analyzer simulates how an operating system handles page faults when
physical memory has fewer frames than a program has pages. It implements
**FIFO**, **LRU** and **Optimal** page replacement by hand, runs them on the
same reference string, and compares hits, faults, ratios and fault counts across
realistic workloads and frame capacities.

> **This is a page-replacement simulator.** It does not measure page faults of
> your running operating system, inspect actual RAM, capture hardware page
> faults, or improve system performance. Every number comes from simulating the
> reference string shown on screen.

## Folder contents

| Path | Description |
|---|---|
| `page_fault_analyzer.c` | Complete single-file source code |
| `page_fault_analyzer` | Linux x86-64 executable built with the command below |
| `README.md` | This file |
| `docs/Page_Fault_Analyzer_Report.docx` | Project report |
| `screenshots/` | Terminal-style renders of real program output (see note below) |
| `tests/input_*.txt`, `tests/output_*.txt` | Scripted test sessions and the output they produced |
| `tests/TEST_LOG.md` | Test cases: input, expected behaviour, actual result, corrections |
| `docs/make_screenshots.py`, `docs/extract_data.py`, `docs/build_report.js` | Helper scripts used to produce screenshots and report |

## Compile and run (GCC)

```bash
gcc -std=c99 -Wall -Wextra -pedantic -O2 -o page_fault_analyzer page_fault_analyzer.c
./page_fault_analyzer              # interactive menu
./page_fault_analyzer --selftest   # built-in verification suite (exit code 0 = all passed)
```

On Windows with MinGW: `gcc -std=c99 -Wall -Wextra -pedantic -O2 -o page_fault_analyzer.exe page_fault_analyzer.c`
then `page_fault_analyzer.exe`.

Replay a recorded test session:

```bash
./page_fault_analyzer < tests/input_03_database_scenario.txt
```

## Menu

| Option | Action |
|---|---|
| 1 | Run FIFO, LRU and Optimal on the current workload (optional step-by-step frame states) |
| 2 | Run one algorithm only |
| 3 | Select a real-world workload scenario (length and seed are configurable) |
| 4 | Enter a custom page-reference sequence |
| 5 | Set the number of memory frames |
| 6 | Frame-capacity experiment (1..N frames, with change per added frame and Belady detection) |
| 7 | Overall comparison of all five workloads under the same frame count |
| 8 | View performance metrics of the last analysis |
| 9 | Real-world interpretation and limitations |
| 10 | Run the built-in self-test |
| 0 | Exit |

Several experiments can be run in one session without restarting.

## Workloads

| Scenario | Models | Generator |
|---|---|---|
| Sequential scan | File scan, media streaming | Pages `0..11` visited in order, looping |
| Repeated access | Database hot working set | 80% of references to 4 hot pages, 20% to 16 cold pages |
| Random access | Many unrelated apps/tabs | Uniform over 20 pages |
| Locality of reference | Desktop multitasking | 85% of references in a 4-page working set that moves every 12 references |
| Memory pressure | Memory-constrained device | Distinct pages = 4 x frames, uniform |
| Custom | Any user string | Typed in, validated |

Random workloads use a built-in xorshift32 generator, so the same seed always
gives the same reference string on every platform (the C `rand()` function is
deliberately not used).

## Limits (preprocessor constants at the top of the source)

`MAX_REFS 200`, `MAX_FRAMES 20`, `MAX_PAGE_NUM 99` (pages are `0..99`),
`LINE_BUF 2048`. All input is validated: bad menu choices, non-numeric tokens,
out-of-range pages, too many references, over-long lines and closed input are
rejected or handled without crashing.

## Algorithms

* **FIFO**: evicts the page that has been resident the longest.
* **LRU**: evicts the page whose last use is the oldest.
* **Optimal**: evicts the page whose next use is farthest away, or never.
  Ties between pages that are never used again go to the lowest frame number.
  It is a theoretical benchmark because it needs knowledge of the future.

Each algorithm is a separate function with its own frame array and counters.

## Verification

* `./page_fault_analyzer --selftest` runs 15 checks: textbook reference strings
  with known results, edge cases, invariants over all workloads, frame counts
  1..20 and three seeds (hits + faults = references, Optimal <= FIFO and LRU,
  faults >= distinct pages, seeded reproducibility), and input-validation checks.
* All scripted sessions in `tests/` were also run under AddressSanitizer and
  UndefinedBehaviorSanitizer with no reports.

## Note on screenshots

The PNG files in `screenshots/` are terminal-style renders produced by
`docs/make_screenshots.py` from the **real captured output** in `tests/`. They
are not OS screen captures. If your course requires literal screenshots, run the
program and capture your own terminal; the same commands are in `tests/`.
