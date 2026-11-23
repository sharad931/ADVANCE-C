// Builds docs/Page_Fault_Analyzer_Report.docx from docs/report_data.json (real program output) and screenshots/.
const fs = require("fs");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell, ImageRun, HeadingLevel,
  AlignmentType, WidthType, ShadingType, BorderStyle, LevelFormat, PageBreak, Footer, PageNumber,
} = require("docx");
const D = JSON.parse(fs.readFileSync("docs/report_data.json", "utf8"));
const W = 9638; // content width (A4, 2 cm margins)

const p = (text, o = {}) => new Paragraph({ spacing: { after: 120, line: 300 }, alignment: o.align, children: [new TextRun({ text, bold: o.bold, italics: o.italics, size: o.size || 22 })], ...(o.extra || {}) });
const rich = (runs, o = {}) => new Paragraph({ spacing: { after: 120, line: 300 }, children: runs.map(r => typeof r === "string" ? new TextRun({ text: r, size: 22 }) : new TextRun({ size: 22, ...r })), ...o });
const h1 = t => new Paragraph({ heading: HeadingLevel.HEADING_1, spacing: { before: 360, after: 160 }, children: [new TextRun({ text: t, bold: true, size: 30, color: "1F3864" })] });
const h2 = t => new Paragraph({ heading: HeadingLevel.HEADING_2, spacing: { before: 240, after: 100 }, children: [new TextRun({ text: t, bold: true, size: 25, color: "2E5597" })] });
const bullet = t => new Paragraph({ numbering: { reference: "bul", level: 0 }, spacing: { after: 60, line: 290 }, children: Array.isArray(t) ? t.map(r => typeof r === "string" ? new TextRun({ text: r, size: 22 }) : new TextRun({ size: 22, ...r })) : [new TextRun({ text: t, size: 22 })] });
const code = lines => lines.map(l => new Paragraph({ spacing: { after: 0, line: 252 }, shading: { type: ShadingType.CLEAR, fill: "F2F2F2" }, children: [new TextRun({ text: l === "" ? " " : l, font: "Consolas", size: 18 })] }));
const caption = t => new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 60, after: 200 }, children: [new TextRun({ text: t, italics: true, size: 19, color: "444444" })] });
const border = { style: BorderStyle.SINGLE, size: 4, color: "999999" };
const borders = { top: border, bottom: border, left: border, right: border };

function table(header, rows, widths, opts = {}) {
  const total = widths.reduce((a, b) => a + b, 0);
  const cell = (txt, w, isHead, align) => new TableCell({
    width: { size: w, type: WidthType.DXA }, borders,
    shading: isHead ? { type: ShadingType.CLEAR, fill: "1F3864" } : undefined,
    margins: { top: 50, bottom: 50, left: 90, right: 90 },
    children: [new Paragraph({ alignment: align, children: [new TextRun({ text: String(txt), bold: isHead, size: 19, color: isHead ? "FFFFFF" : "000000" })] })],
  });
  const al = i => (opts.left && opts.left.includes(i)) ? AlignmentType.LEFT : AlignmentType.CENTER;
  return new Table({
    width: { size: total, type: WidthType.DXA }, columnWidths: widths,
    rows: [new TableRow({ tableHeader: true, children: header.map((h, i) => cell(h, widths[i], true, al(i))) }),
      ...rows.map(r => new TableRow({ cantSplit: true, children: r.map((c, i) => cell(c, widths[i], false, al(i))) }))],
  });
}
function img(file, widthPx = 560) {
  const buf = fs.readFileSync(file);
  const w = buf.readUInt32BE(16), h = buf.readUInt32BE(20);
  const hh = Math.round(widthPx * h / w);
  return new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 120, after: 40 }, keepNext: true,
    children: [new ImageRun({ type: "png", data: buf, transformation: { width: widthPx, height: hh }, altText: { title: file, description: "Program output", name: file } })] });
}
const sp = () => new Paragraph({ spacing: { after: 80 }, children: [] });

const algoRows = c => c.rows.map(r => [r.alg, r.refs, r.hits, r.faults, r.hr, r.fr, r.pct + "%"]);
const algoHead = ["Algorithm", "Refs", "Hits", "Faults", "Hit ratio", "Fault ratio", "Fault %"];
const algoW = [1700, 1000, 1000, 1200, 1500, 1738, 1500];
const expHead = ["Frames", "FIFO", "LRU", "Optimal", "FIFO-Opt", "LRU-Opt", "Lowest"];
const expW = [900, 1450, 1450, 1450, 1150, 1150, 2088];

const kids = [];
// ---------------- Cover ----------------
kids.push(sp(), sp(), sp());
kids.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 200 }, children: [new TextRun({ text: "PROJECT REPORT", size: 26, bold: true, color: "666666" })] }));
kids.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 120 }, children: [new TextRun({ text: "Page Fault Analyzer", size: 64, bold: true, color: "1F3864" })] }));
kids.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 600 }, children: [new TextRun({ text: "A Console-Based Virtual-Memory Page Replacement Simulator in C", size: 28, color: "2E5597" })] }));
kids.push(p("Subject: Advanced C Programming", { align: AlignmentType.CENTER, size: 24 }));
kids.push(p("Algorithms: FIFO, LRU and Optimal Page Replacement", { align: AlignmentType.CENTER, size: 24 }));
kids.push(p("Source file: page_fault_analyzer.c (standard C99, GCC)", { align: AlignmentType.CENTER, size: 24 }));
kids.push(sp(), sp());
kids.push(p("Submitted by: Nirav", { align: AlignmentType.CENTER, size: 24, bold: true }));
kids.push(p("B.E. Computer Science & Engineering, Chandigarh University, Mohali", { align: AlignmentType.CENTER, size: 22 }));
kids.push(p("UID: 24BCS10124", { align: AlignmentType.CENTER, size: 22 }));
kids.push(new Paragraph({ children: [new PageBreak()] }));

// ---------------- 1. Abstract ----------------
kids.push(h1("1. Abstract"));
kids.push(p("When a program uses more pages than the physical memory has frames, the operating system must decide which resident page to evict whenever a missing page is referenced. Each missing reference is a page fault and is expensive because the page must be fetched from storage. This project, the Page Fault Analyzer, is a console application written in raw C that simulates this behaviour. It implements FIFO, LRU and Optimal page replacement as separate hand-written functions and runs all of them on exactly the same page-reference string."));
kids.push(p("The tool goes beyond entering a number sequence: it provides five synthetic workloads inspired by real systems (sequential file scan, database hot set, random multi-application access, locality of reference, and memory pressure), accepts custom reference strings, supports seeded and therefore reproducible random workloads, shows the frame contents after every reference, calculates hit and fault ratios, compares frame capacities, detects Belady's anomaly, and produces data-driven interpretations. All reported values in this document were produced by the actual program."));
kids.push(p("Important scope statement: the application is a page-replacement simulator. It does not measure page faults in the user's running operating system, does not inspect real RAM and does not capture hardware faults."));

// ---------------- 2. Problem statement ----------------
kids.push(h1("2. Problem Statement"));
kids.push(p("Textbook examples of page replacement usually show a single short reference string and a fault count. That hides the questions that matter in practice: how does the access pattern change which algorithm is best, how many frames does a workload need, and how far are practical algorithms from the theoretical optimum? The problem addressed here is to build a configurable, verifiable simulator in C that answers those questions with measurable output, for several realistic access patterns, under identical conditions for all algorithms."));

// ---------------- 3. Objectives ----------------
kids.push(h1("3. Objectives"));
["Implement FIFO, LRU and Optimal page replacement independently in standard C with separate frame arrays, counters and replacement logic.",
 "Model realistic scenarios as executable workload generators: sequential, repeated access, random, locality of reference and memory pressure, plus custom input.",
 "Show every step: the current reference, the state of every frame, hit or fault and the evicted page.",
 "Compute hits, faults, hit ratio, fault ratio and fault percentage dynamically; report all algorithms that reach the lowest fault count.",
 "Provide experiments on frame capacity and on workload type, including the change in faults for each added frame.",
 "Validate all input, stay within array bounds and never terminate unexpectedly.",
 "Verify correctness with built-in self-tests, scripted sessions and sanitizer runs.",
 "State clearly what a simulator can and cannot say about a real operating system."].forEach(t => kids.push(bullet(t)));

// ---------------- 4. System design ----------------
kids.push(h1("4. System Design"));
kids.push(h2("4.1 Module structure"));
kids.push(table(["Module", "Main functions", "Purpose"], [
  ["Input and validation", "read_line, parse_long, prompt_long, prompt_yes_no, parse_sequence", "Safe line reading, strict integer parsing, range checks, re-prompting, clean exit on end of input"],
  ["Workload generation", "generate_workload, rng_seed, rng_next, rng_below, count_distinct", "Five predefined generators using a portable seeded random generator; distinct-page counting"],
  ["Frame helpers", "init_frames, find_page, trace_header, trace_step", "Frame initialisation, page search, step-by-step display"],
  ["Algorithms", "run_fifo, run_lru, run_optimal, next_use", "The three replacement policies, each with its own frames and counters"],
  ["Metrics", "reset_result, finalize_result, verify_results, list_lowest", "Hit/fault totals and ratios (division by zero guarded), self-checks, ties for the lowest fault count"],
  ["Reporting", "print_config, print_results_table, print_comparison, print_interpretation, print_limitations", "Banner, tables, comparison and data-driven interpretation"],
  ["Experiments", "action_frame_experiment, action_workload_comparison", "Frame-capacity study and all-workload report"],
  ["Application", "main, print_menu, action_* functions, run_selftest", "Menu loop, state handling, self-test"],
], [2000, 3700, 3938], { left: [0, 1, 2] }));
kids.push(sp());
kids.push(h2("4.2 Data structures"));
kids.push(bullet([{ text: "Workload", bold: true }, " : kind, name, real-world description, refs[MAX_REFS], count, distinct, seed."]));
kids.push(bullet([{ text: "Result", bold: true }, " : algorithm name, total, hits, faults, hit ratio, fault ratio, fault percentage."]));
kids.push(bullet([{ text: "AppState / LastRun", bold: true }, " : current workload, frame count, generation length and seed, and the last analysis shown by menu option 8."]));
kids.push(bullet([{ text: "Frame table", bold: true }, " : a plain int array of MAX_FRAMES elements per algorithm (EMPTY_FRAME = -1), so results stay independent."]));
kids.push(h2("4.3 Limits (preprocessor constants)"));
kids.push(table(["Constant", "Value", "Meaning"], [
  ["MAX_REFS", "200", "Maximum references per string (requirement: at least 100)"],
  ["MAX_FRAMES", "20", "Maximum frames (requirement: at least 20)"],
  ["MAX_PAGE_NUM", "99", "Valid page numbers are 0 to 99"],
  ["LINE_BUF", "2048", "Input line buffer size"],
], [2600, 1400, 5638], { left: [0, 2] }));
kids.push(sp());
kids.push(h2("4.4 Menu"));
kids.push(p("The welcome screen shows the project title and a notice that the program is a simulator. The menu offers: run all algorithms, run one algorithm, select a workload scenario, enter a custom sequence, set the number of frames, compare frame capacities, compare all workloads, view performance metrics, interpretation and limitations, self-test, and exit. Any number of experiments can be performed in one session."));

// ---------------- 5. Algorithms ----------------
kids.push(h1("5. Algorithm Explanations and Pseudocode"));
kids.push(h2("5.1 FIFO"));
kids.push(p("FIFO treats the frames as a queue. The page that entered memory earliest is evicted first, regardless of how often it has been used. The implementation fills frames 0 to n-1 in order and then moves a circular pointer to the oldest page. FIFO is simple but can evict a heavily used page and can exhibit Belady's anomaly."));
kids.push(...code(["for each reference page p:", "    if p is in frames:  hits++", "    else:", "        faults++", "        if a frame is empty: place p there", "        else: evict frames[oldest]; place p; oldest = (oldest + 1) mod n"]));
kids.push(h2("5.2 LRU"));
kids.push(p("LRU assumes that pages used recently will be used again soon. Each loaded page has a last-use time stamp that is refreshed on every hit. On a fault with full memory, the page with the smallest time stamp is evicted. Frequently touched pages therefore tend to stay in memory."));
kids.push(...code(["for each reference page p at time t:", "    if p is in frames:  hits++; last_used[p] = t", "    else:", "        faults++", "        if a frame is empty: use it", "        else: victim = frame with smallest last_used; evict it", "        place p; last_used[p] = t"]));
kids.push(h2("5.3 Optimal (benchmark)"));
kids.push(p("Optimal looks ahead in the reference string and evicts the page whose next use is farthest away, or which is never used again. It produces the minimum possible number of faults for a given string and frame count, but a real operating system cannot know future references, so it is used only as a benchmark. When several resident pages are never used again, the implementation evicts the one in the lowest frame number."));
kids.push(...code(["for each reference page p at position i:", "    if p is in frames:  hits++", "    else:", "        faults++", "        if a frame is empty: use it", "        else:", "            for each resident page q: next[q] = first position > i where q occurs, or INFINITY", "            evict the page with the largest next[q]", "        place p"]));
kids.push(h2("5.4 Main flow"));
kids.push(...code(["start -> show welcome screen", "loop:", "    show menu; read choice (validated, 0-10)", "    1/2 : print configuration -> run algorithm(s) -> table -> comparison -> self-check -> interpretation", "    3/4 : build workload (generator or validated custom string) and show configuration", "    5   : set frames (regenerate memory-pressure workload if selected)", "    6   : for frames = 1..N run all algorithms silently -> table, deltas, Belady detection", "    7   : for each of the five workloads run all algorithms -> per-workload tables + summary", "    8/9/10 : metrics / limitations / self-test", "    0   : exit"]));

// ---------------- 6. Real-world scenarios ----------------
kids.push(h1("6. Real-World Scenarios"));
kids.push(p("Each scenario is an executable generator, not only a description. The parameters below are constants in the source and are printed in the program banner."));
kids.push(table(["Scenario", "Real-world analogy", "How references are generated"], [
  ["Sequential scan", "File scan, media streaming", "Pages 0 to 11 visited in order, looping until the requested length"],
  ["Repeated access", "Database server re-reading a hot working set", "80% of references go to 4 hot pages; 20% to 16 cold pages"],
  ["Random access", "Many unrelated browser tabs or applications", "Uniform random over 20 pages"],
  ["Locality of reference", "Desktop multitasking with a moving working set", "85% of references inside a 4-page window that moves every 12 references; the rest anywhere in 20 pages"],
  ["Memory pressure", "Memory-constrained device", "Distinct pages = 4 x number of frames, uniform random"],
  ["Custom", "Any trace the user enters", "Validated list of page numbers 0-99"],
], [2100, 3000, 4538], { left: [0, 1, 2] }));
kids.push(sp());
kids.push(p("Random workloads use a built-in xorshift32 generator initialised from a user-chosen seed. The same seed gives the same reference string on every platform, which makes experiments repeatable. The standard rand() function is deliberately avoided because its sequence is implementation-defined."));

// ---------------- 7. Implementation methodology ----------------
kids.push(h1("7. Implementation Methodology"));
["Requirements were turned into a checklist and mapped to functions (one function per responsibility, no core logic in main).",
 "The three algorithms were written first and checked against well-known textbook strings with known fault counts.",
 "Workload generators and the seeded random generator were added next, then the step-by-step trace and metrics.",
 "Experiments (frame capacity, all workloads), interpretation text and the self-test were added, with all numbers computed from the run.",
 "Input handling uses fgets and strtol with full-string validation; over-long lines are discarded; end of input exits cleanly.",
 "The program was compiled with -std=c99 -Wall -Wextra -pedantic (no warnings) and all sessions were re-run under AddressSanitizer and UndefinedBehaviorSanitizer."].forEach(t => kids.push(bullet(t)));
kids.push(p("Design decisions that matter for correctness:"));
kids.push(bullet("Each algorithm has its own frame array, so one algorithm cannot influence another."));
kids.push(bullet("All algorithms read the same Workload structure, so differences come only from the replacement policy."));
kids.push(bullet("The distinct-page count and all ratios are calculated from the data; ratios are guarded against a zero total."));
kids.push(bullet("The interpretation text only states comparisons that the numbers support (for example it says FIFO beat LRU when that is what the data shows)."));

// ---------------- 8. Test cases ----------------
kids.push(h1("8. Test Cases"));
kids.push(p("The complete log is in tests/TEST_LOG.md. Selected cases:"));
kids.push(table(["Test", "Input", "Expected", "Actual"], [
  ["Textbook string", "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1, 3 frames", "FIFO 15, LRU 12, Optimal 9", "15 / 12 / 9"],
  ["Belady string", "1 2 3 4 1 2 5 1 2 3 4 5, 3 then 4 frames", "FIFO rises 9 to 10; LRU 10 to 8; Optimal 7 to 6", "As expected; anomaly reported"],
  ["Single reference", "9, 1 frame", "1 fault", "1 / 1 / 1"],
  ["Single frame", "1 2 1 2 1 2", "Every reference faults", "6 / 6 / 6"],
  ["More frames than pages", "1 2 3 1 2 3, 5 frames", "3 compulsory faults only", "3 / 3 / 3"],
  ["Invariants", "5 workloads x frames 1-20 x 3 seeds", "hits+faults = refs; Optimal <= FIFO, LRU; faults >= distinct", "Pass"],
  ["Reproducibility", "Seed 99 twice; seed 100", "Same output; different output", "Identical; different"],
  ["Invalid menu / frames", "abc, 99, -1; frames 0 and 21", "Rejected with message", "Rejected"],
  ["Invalid sequence", "1 2 x; 1 2 200; 1 -3; 201 values; 5000-character line", "Rejected, previous workload kept", "Rejected"],
  ["End of input", "Input stream closed mid-prompt", "Clean exit", "Exit code 0"],
  ["Sanitizers", "All sessions with ASan + UBSan", "No reports", "None"],
], [1700, 3200, 2638, 2100], { left: [0, 1, 2, 3] }));
kids.push(sp());
kids.push(h2("8.1 Built-in self-test result"));
kids.push(p(D.selftest_summary + " (command: ./page_fault_analyzer --selftest). Individual checks:"));
kids.push(...code(D.selftest.map(s => s.length > 110 ? s.slice(0, 107) + "..." : s)));
kids.push(h2("8.2 Corrections made during testing"));
kids.push(bullet("Empty frames in the step table were printed with different alignment from occupied frames; corrected so columns line up."));
kids.push(bullet("Singular/plural wording (for example '1 faults') was changed to fault(s), reference(s) and capacity(ies)."));
kids.push(bullet("No defect was found in the replacement logic itself."));

// ---------------- 9. Screenshots ----------------
kids.push(h1("9. Output Screenshots"));
kids.push(p("The images below are terminal-style renders of the real output captured in the tests folder (generated by docs/make_screenshots.py). They are not operating-system screen captures; students who need literal captures can reproduce them with the commands in the README."));
const shots = [
  ["01_welcome_and_menu.png", "Figure 1: Welcome screen and menu", 560],
  ["02_step_by_step_fifo_lru.png", "Figure 2: Step-by-step frame states (FIFO and LRU, textbook string, 3 frames)", 520],
  ["03_comparison_table_textbook.png", "Figure 3: Comparison table with lowest fault count and self-check", 560],
  ["04_database_scenario_interpretation.png", "Figure 4: Database (repeated-access) scenario with interpretation", 560],
  ["05_frame_capacity_experiment_db.png", "Figure 5: Frame-capacity experiment on the database workload", 600],
  ["06_belady_anomaly.png", "Figure 6: Belady's anomaly detected for FIFO", 600],
  ["07_workload_comparison.png", "Figure 7: Overall workload comparison summary", 600],
  ["08_sequential_scan_scenario.png", "Figure 8: Sequential scan scenario (FIFO and LRU fault on every reference)", 560],
  ["09_self_test.png", "Figure 9: Built-in self-test", 600],
  ["10_input_validation.png", "Figure 10: Input validation messages", 560],
];
shots.forEach(([f, c, w]) => { kids.push(img("screenshots/" + f, w)); kids.push(caption(c)); });

// ---------------- 10. Performance comparison ----------------
kids.push(h1("10. Performance Comparison and Sample Experiments"));
kids.push(h2("10.1 Textbook reference string (3 frames)"));
kids.push(table(algoHead, algoRows(D.textbook), algoW));
kids.push(p("Lowest fault count: " + D.textbook.lowest + ". Efficiency relative to Optimal (menu option 8): " + D.textbook_metrics.map(m => `${m[0]} ${m[4]}`).join(", ") + "."));
kids.push(h2("10.2 Overall workload comparison (4 frames, 60 references, seed 42)"));
kids.push(p("Menu option 7 produces the following report. Each workload is listed with hits, faults and fault ratio per algorithm."));
D.overall_blocks.forEach(b => {
  kids.push(rich([{ text: b.name, bold: true }, "  (" + b.info + ")"]));
  kids.push(table(["Algorithm", "Hits", "Faults", "Fault ratio"], b.rows, [2600, 2000, 2000, 3038]));
  kids.push(p("Lowest fault count: " + b.lowest, { italics: true, size: 20 }));
});
kids.push(h2("10.3 Summary of faults by workload"));
kids.push(table(["Workload", "Refs", "Distinct", "FIFO", "LRU", "Optimal", "Lowest", "FIFO vs LRU"], D.overall, [1500, 800, 1000, 900, 900, 1100, 1438, 2000]));
kids.push(sp());
kids.push(p("Reading the table: the sequential scan produces the same number of faults for FIFO and LRU (both fault on every reference, because the scan loops over 12 pages with only 4 frames), while Optimal avoids some faults by keeping pages that will be needed soonest. In the repeated-access and locality workloads LRU had fewer faults than FIFO. For the random workload FIFO and LRU tied. These statements are read from the program output, not assumed."));
kids.push(h2("10.4 Frame-capacity experiment: database workload (repeated access, seed 42)"));
kids.push(table(expHead, D.db_exp.rows, expW));
kids.push(p(D.db_exp.obs.slice(0, 3).join(" "), { size: 20 }));
kids.push(h2("10.5 Frame-capacity experiment: memory-pressure workload (80 references, seed 7)"));
kids.push(table(expHead, D.pressure_exp.rows, expW));
kids.push(p(D.pressure_exp.obs.slice(0, 3).join(" "), { size: 20 }));
kids.push(p("Here FIFO and LRU are close and neither dominates: at 4 frames FIFO had " + D.pressure.rows[0].faults + " faults against LRU's " + D.pressure.rows[1].faults + ". This confirms that LRU does not always beat FIFO and that the comparison must be decided by the data."));
kids.push(h2("10.6 Belady's anomaly (custom string 1 2 3 4 1 2 5 1 2 3 4 5)"));
kids.push(table(expHead, D.belady.rows, expW));
kids.push(p(D.belady.obs.slice(0, 2).join(" "), { size: 20 }));
kids.push(p("FIFO produced more faults with 4 frames (10) than with 3 frames (9). LRU and Optimal, which are stack algorithms, did not show this behaviour."));
kids.push(h2("10.7 Other scenario runs"));
kids.push(table(["Scenario", "Frames", "FIFO", "LRU", "Optimal", "Lowest"], [
  ["Database hot set (60 refs)", "4", D.db.rows[0].faults, D.db.rows[1].faults, D.db.rows[2].faults, D.db.lowest],
  ["Locality (60 refs, seed 7)", "3", D.locality.rows[0].faults, D.locality.rows[1].faults, D.locality.rows[2].faults, D.locality.lowest],
  ["Sequential scan (36 refs)", "3", D.sequential.rows[0].faults, D.sequential.rows[1].faults, D.sequential.rows[2].faults, D.sequential.lowest],
  ["Memory pressure (80 refs, seed 7)", "4", D.pressure.rows[0].faults, D.pressure.rows[1].faults, D.pressure.rows[2].faults, D.pressure.lowest],
], [3000, 1000, 1200, 1200, 1300, 1938], { left: [0] }));
kids.push(sp());

// ---------------- 11. Real-world interpretation ----------------
kids.push(h1("11. Real-World Interpretation"));
kids.push(bullet("A repeated-access workload represents a database or application re-reading a working set. When LRU produces fewer faults than FIFO, it is because each access refreshes a page's recency, so frequently used pages stay resident, whereas FIFO evicts the oldest-loaded page even if it is used constantly."));
kids.push(bullet("A locality workload approximates programs that keep revisiting recently used memory; the data shows how close LRU and FIFO come to the Optimal benchmark."));
kids.push(bullet("A memory-pressure workload shows the effect of too few frames. A high fault rate suggests the available frames are insufficient for the simulated workload; the frame-capacity experiment shows how quickly faults fall as frames are added."));
kids.push(bullet("A sequential scan that loops over more pages than there are frames defeats both FIFO and LRU; every page is evicted just before it is needed again."));
kids.push(bullet("Optimal is a theoretical benchmark because it uses knowledge of future references, which a real operating system generally does not have."));

// ---------------- 12. Limitations ----------------
kids.push(h1("12. Limitations"));
["This is a page-replacement simulator. It does not measure page faults of the running operating system, inspect actual RAM, capture hardware faults or improve system performance.",
 "Real systems add caching, working-set tracking, prefetching, dirty-page write-back, storage latency and memory-allocation policies that are not modelled.",
 "Workloads are synthetic with fixed parameters and are not recorded traces from real programs.",
 "Only fixed-size frames for a single process are modelled; there is no multi-process frame allocation.",
 "Only FIFO, LRU and Optimal are implemented; other policies such as Clock or LFU are not included.",
 "Limits are 200 references, 20 frames and page numbers 0 to 99."].forEach(t => kids.push(bullet(t)));

// ---------------- 13. Future scope ----------------
kids.push(h1("13. Future Scope"));
["Add the Clock (second-chance) and LFU algorithms and compare them with LRU.",
 "Load reference strings from a trace file, for example a recorded sequence of page accesses.",
 "Model multiple processes with global versus local frame allocation and a working-set model.",
 "Add a simple cost model with separate hit and fault latency to estimate average access time.",
 "Export experiment tables to CSV for plotting in a spreadsheet.",
 "Add a text-based chart of faults against frame count."].forEach(t => kids.push(bullet(t)));

// ---------------- 14. Conclusion ----------------
kids.push(h1("14. Conclusion"));
kids.push(p("The Page Fault Analyzer implements FIFO, LRU and Optimal page replacement in standard C and applies them to five realistic workload models and to custom input. Results are computed from the data, are reproducible through seeded generation, and are verified by 15 built-in checks, scripted sessions and sanitizer runs. The experiments show that the best practical policy depends on the access pattern: in the 4-frame, seed-42 comparison LRU had fewer faults than FIFO on the repeated-access, locality and memory-pressure workloads, FIFO and LRU were equal on the sequential scan and random workloads, FIFO was slightly better than LRU on a different memory-pressure run (seed 7), and FIFO exhibited Belady's anomaly on the classic string. Optimal always gave the lowest count, as expected of a theoretical benchmark. The tool is a simulator and its conclusions apply to the simulated reference strings, not to measurements of a real operating system."));

// ---------------- Appendix ----------------
kids.push(h1("Appendix A: Compile and Run Commands"));
kids.push(...code(["gcc -std=c99 -Wall -Wextra -pedantic -O2 -o page_fault_analyzer page_fault_analyzer.c", "./page_fault_analyzer", "./page_fault_analyzer --selftest", "./page_fault_analyzer < tests/input_03_database_scenario.txt"]));

const doc = new Document({
  creator: "Nirav", title: "Page Fault Analyzer - Project Report",
  styles: { default: { document: { run: { font: "Calibri", size: 22 } } } },
  numbering: { config: [{ reference: "bul", levels: [{ level: 0, format: LevelFormat.BULLET, text: "•", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 540, hanging: 270 } } } }] }] },
  sections: [{
    properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1134, bottom: 1134, left: 1134, right: 1134 } } },
    footers: { default: new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Page Fault Analyzer  |  Page ", size: 18, color: "777777" }), new TextRun({ children: [PageNumber.CURRENT], size: 18, color: "777777" })] })] }) },
    children: kids,
  }],
});
Packer.toBuffer(doc).then(b => { fs.writeFileSync("docs/Page_Fault_Analyzer_Report.docx", b); console.log("report written", b.length); });
