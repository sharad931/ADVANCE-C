/*
 * ============================================================================
 *  PAGE FAULT ANALYZER
 *  Subject : Advanced C Programming
 *  Type    : Console-based memory-management (page replacement) simulator
 *  Language: Standard C (C99), only the C standard library is used
 *
 *  What it does
 *  ------------
 *  Simulates how an operating system handles page faults when the number of
 *  physical memory frames is smaller than the number of pages a program uses.
 *  Three replacement policies are implemented by hand and compared on
 *  identical reference strings:
 *
 *      FIFO     - evict the page that has been in memory the longest
 *      LRU      - evict the page whose last use is the oldest
 *      Optimal  - evict the page whose next use is farthest in the future
 *                 (a theoretical benchmark; it needs knowledge of the future)
 *
 *  IMPORTANT: this is a page-replacement SIMULATOR. It does not read real
 *  RAM, does not capture hardware page faults and does not change how the
 *  operating system behaves. All numbers come from simulating the page
 *  reference string that is shown on screen.
 *
 *  Compile : gcc -std=c99 -Wall -Wextra -pedantic -O2 -o page_fault_analyzer page_fault_analyzer.c
 *  Run     : ./page_fault_analyzer            (interactive menu)
 *            ./page_fault_analyzer --selftest (built-in verification suite)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <stdint.h>

/* ------------------------------ Limits & constants ----------------------- */
#define MAX_REFS        200      /* maximum length of a reference string (>= 100)   */
#define MAX_FRAMES      20       /* maximum number of physical frames (>= 20)       */
#define MAX_PAGE_NUM    99       /* valid page numbers are 0 .. MAX_PAGE_NUM        */
#define NUM_PAGES       (MAX_PAGE_NUM + 1)
#define LINE_BUF        2048     /* size of the line input buffer                   */
#define NUM_ALGOS       3
#define NUM_SCENARIOS   5        /* predefined workloads (custom is extra)          */
#define DEFAULT_FRAMES  3
#define DEFAULT_REFS    60
#define DEFAULT_SEED    2024
#define MAX_SEED        2147483647L
#define EMPTY_FRAME     (-1)
#define NEVER_USED      INT_MAX

/* Workload model parameters (used by the generators and by the menu text) */
#define SEQ_PAGES       12       /* sequential: loop over pages 0..11               */
#define HOT_PAGES       4        /* repeated  : size of the hot set                 */
#define COLD_PAGES      16       /* repeated  : size of the cold pool               */
#define HOT_PERCENT     80       /* repeated  : % of accesses that hit the hot set  */
#define RANDOM_POOL     20       /* random    : uniform over this many pages        */
#define LOC_POOL        20       /* locality  : total pages in the address space    */
#define LOC_WINDOW      4        /* locality  : size of the current working set     */
#define LOC_PHASE       12       /* locality  : working set moves every N refs      */
#define LOC_STAY_PERCENT 85      /* locality  : % of refs inside the working set    */
#define PRESSURE_FACTOR 4        /* pressure  : distinct pages = factor * frames    */

/* --------------------------------- Types --------------------------------- */
typedef enum {
    WL_SEQUENTIAL = 0,
    WL_REPEATED,
    WL_RANDOM,
    WL_LOCALITY,
    WL_PRESSURE,
    WL_CUSTOM
} WorkloadKind;

typedef struct {
    WorkloadKind kind;
    char         name[48];        /* short workload name                       */
    char         scenario[128];   /* real-world situation it models            */
    int          refs[MAX_REFS];  /* the page reference string                 */
    int          count;           /* number of references                      */
    int          distinct;        /* number of distinct pages in the string    */
    unsigned int seed;            /* seed used (random-based workloads only)   */
    int          seeded;          /* 1 if 'seed' is meaningful                 */
} Workload;

typedef struct {
    const char *name;
    int    total;       /* references processed              */
    int    hits;
    int    faults;
    double hit_ratio;   /* hits   / total                    */
    double fault_ratio; /* faults / total                    */
    double fault_pct;   /* fault_ratio * 100                 */
} Result;

typedef Result (*AlgoFn)(const Workload *w, int frames, int verbose);

typedef struct {
    int    valid;                 /* 1 once an analysis has been run  */
    char   name[48];
    int    refs, distinct, frames;
    int    have[NUM_ALGOS];       /* which algorithms were run        */
    Result res[NUM_ALGOS];
} LastRun;

typedef struct {
    Workload     workload;        /* currently selected workload      */
    int          frames;          /* currently selected frame count   */
    int          gen_refs;        /* length used by generated loads   */
    unsigned int seed;            /* seed used by generated loads     */
    LastRun      last;
} AppState;

/* Small, portable, repeatable pseudo random generator (xorshift32).
 * rand() is deliberately NOT used: its sequence differs between platforms,
 * which would make "reproducible" experiments non-reproducible. */
typedef struct { uint32_t state; } Rng;

/* ----------------------------- Forward declarations ---------------------- */
static Result run_fifo(const Workload *w, int frames, int verbose);
static Result run_lru(const Workload *w, int frames, int verbose);
static Result run_optimal(const Workload *w, int frames, int verbose);

static const char *ALGO_NAMES[NUM_ALGOS] = { "FIFO", "LRU", "Optimal" };
static const AlgoFn ALGO_FUNCS[NUM_ALGOS] = { run_fifo, run_lru, run_optimal };

/* ============================================================================
 *  SECTION 1 - Input handling and validation
 * ========================================================================== */

/* Read one line safely. Returns 1 on success, 0 if the line was too long
 * (the rest of the line is discarded). On end-of-input the program exits
 * cleanly instead of looping forever. */
static int read_line(char *buf, size_t size)
{
    size_t len;
    int c, overflow = 0;

    if (fgets(buf, (int)size, stdin) == NULL) {
        printf("\n[Input closed - exiting.]\n");
        exit(EXIT_SUCCESS);
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    } else {
        /* no newline: the line did not fit (or EOF without newline) */
        while ((c = getchar()) != '\n' && c != EOF)
            overflow = 1;
    }
    if (len > 0 && buf[len - 1] == '\r')
        buf[--len] = '\0';
    return overflow ? 0 : 1;
}

/* Strict integer parser: whole string must be a decimal integer. */
static int parse_long(const char *s, long *out)
{
    char *end;
    long v;

    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return 0;
    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || errno == ERANGE) return 0;
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

static int is_blank(const char *s)
{
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Prompt until the user types an integer in [lo, hi].
 * If has_def is set, an empty line returns def. */
static long prompt_long(const char *prompt, long lo, long hi, long def, int has_def)
{
    char line[LINE_BUF];
    long v;

    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!read_line(line, sizeof line)) {
            printf("  Invalid input: line too long. Please try again.\n");
            continue;
        }
        if (has_def && is_blank(line)) return def;
        if (!parse_long(line, &v)) {
            printf("  Invalid input: please enter a whole number.\n");
            continue;
        }
        if (v < lo || v > hi) {
            printf("  Invalid input: value must be between %ld and %ld.\n", lo, hi);
            continue;
        }
        return v;
    }
}

/* Yes/No prompt. Empty input returns def_yes. */
static int prompt_yes_no(const char *prompt, int def_yes)
{
    char line[LINE_BUF];

    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!read_line(line, sizeof line)) {
            printf("  Invalid input: line too long.\n");
            continue;
        }
        if (is_blank(line)) return def_yes;
        if (line[0] == 'y' || line[0] == 'Y') return 1;
        if (line[0] == 'n' || line[0] == 'N') return 0;
        printf("  Please answer y or n.\n");
    }
}

/* ============================================================================
 *  SECTION 2 - Workload generation
 * ========================================================================== */

static void rng_seed(Rng *r, unsigned int seed)
{
    r->state = (uint32_t)seed * 2654435761u ^ 0x9E3779B9u;
    if (r->state == 0) r->state = 0xA5A5A5A5u;
}

static uint32_t rng_next(Rng *r)
{
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

/* Uniform-ish integer in [0, n). n must be > 0. */
static int rng_below(Rng *r, int n)
{
    return (int)(rng_next(r) % (uint32_t)n);
}

/* Count how many different pages appear in the reference string. */
static int count_distinct(const int refs[], int count)
{
    int seen[NUM_PAGES] = {0};
    int i, d = 0;

    for (i = 0; i < count; i++) {
        if (refs[i] >= 0 && refs[i] <= MAX_PAGE_NUM && !seen[refs[i]]) {
            seen[refs[i]] = 1;
            d++;
        }
    }
    return d;
}

/* Build one of the predefined workloads. 'frames' is only used by the
 * memory-pressure workload (distinct pages scale with the frame count). */
static void generate_workload(Workload *w, WorkloadKind kind, int count,
                              unsigned int seed, int frames)
{
    Rng rng;
    int i, base, pool;

    if (count < 1) count = 1;
    if (count > MAX_REFS) count = MAX_REFS;

    memset(w, 0, sizeof *w);
    w->kind = kind;
    w->count = count;
    w->seed = seed;
    w->seeded = (kind != WL_SEQUENTIAL);
    rng_seed(&rng, seed);

    switch (kind) {
    case WL_SEQUENTIAL:
        snprintf(w->name, sizeof w->name, "Sequential scan");
        snprintf(w->scenario, sizeof w->scenario,
                 "File scan / media streaming: pages visited in order, looping over %d pages",
                 SEQ_PAGES);
        for (i = 0; i < count; i++)
            w->refs[i] = i % SEQ_PAGES;
        break;

    case WL_REPEATED:
        snprintf(w->name, sizeof w->name, "Repeated access (hot set)");
        snprintf(w->scenario, sizeof w->scenario,
                 "Database server: %d%% of requests hit %d hot data pages, the rest spread over %d cold pages",
                 HOT_PERCENT, HOT_PAGES, COLD_PAGES);
        for (i = 0; i < count; i++) {
            if (rng_below(&rng, 100) < HOT_PERCENT)
                w->refs[i] = rng_below(&rng, HOT_PAGES);
            else
                w->refs[i] = HOT_PAGES + rng_below(&rng, COLD_PAGES);
        }
        break;

    case WL_RANDOM:
        snprintf(w->name, sizeof w->name, "Random access");
        snprintf(w->scenario, sizeof w->scenario,
                 "Many unrelated browser tabs/apps: uniformly random pages out of %d",
                 RANDOM_POOL);
        for (i = 0; i < count; i++)
            w->refs[i] = rng_below(&rng, RANDOM_POOL);
        break;

    case WL_LOCALITY:
        snprintf(w->name, sizeof w->name, "Locality of reference");
        snprintf(w->scenario, sizeof w->scenario,
                 "Desktop multitasking: %d%% of references stay in a %d-page working set that shifts every %d references",
                 LOC_STAY_PERCENT, LOC_WINDOW, LOC_PHASE);
        base = rng_below(&rng, LOC_POOL - LOC_WINDOW + 1);
        for (i = 0; i < count; i++) {
            if (i > 0 && i % LOC_PHASE == 0)
                base = rng_below(&rng, LOC_POOL - LOC_WINDOW + 1);
            if (rng_below(&rng, 100) < LOC_STAY_PERCENT)
                w->refs[i] = base + rng_below(&rng, LOC_WINDOW);
            else
                w->refs[i] = rng_below(&rng, LOC_POOL);
        }
        break;

    case WL_PRESSURE:
        pool = frames * PRESSURE_FACTOR;
        if (pool > NUM_PAGES) pool = NUM_PAGES;
        if (pool < 2) pool = 2;
        snprintf(w->name, sizeof w->name, "Memory pressure");
        snprintf(w->scenario, sizeof w->scenario,
                 "Memory-constrained device: %d distinct pages compete for only %d frames",
                 pool, frames);
        for (i = 0; i < count; i++)
            w->refs[i] = rng_below(&rng, pool);
        break;

    default:
        break;
    }
    w->distinct = count_distinct(w->refs, w->count);
}

/* Parse a user-typed reference string such as "7 0 1 2, 0 3". On error the
 * workload is left untouched and a message is written to err. */
static int parse_sequence(const char *line, Workload *w, char *err, size_t errsz)
{
    char tmp[LINE_BUF];
    int refs[MAX_REFS];
    int n = 0;
    char *tok;
    long v;

    strncpy(tmp, line, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = '\0';

    for (tok = strtok(tmp, " ,;\t"); tok != NULL; tok = strtok(NULL, " ,;\t")) {
        if (!parse_long(tok, &v)) {
            snprintf(err, errsz, "'%s' is not a valid whole number.", tok);
            return 0;
        }
        if (v < 0 || v > MAX_PAGE_NUM) {
            snprintf(err, errsz, "page number %ld is out of range (0-%d).", v, MAX_PAGE_NUM);
            return 0;
        }
        if (n >= MAX_REFS) {
            snprintf(err, errsz, "too many references (maximum is %d).", MAX_REFS);
            return 0;
        }
        refs[n++] = (int)v;
    }
    if (n == 0) {
        snprintf(err, errsz, "no page numbers were entered.");
        return 0;
    }

    memset(w, 0, sizeof *w);
    w->kind = WL_CUSTOM;
    snprintf(w->name, sizeof w->name, "Custom sequence");
    snprintf(w->scenario, sizeof w->scenario, "User-supplied page reference string");
    memcpy(w->refs, refs, (size_t)n * sizeof(int));
    w->count = n;
    w->distinct = count_distinct(w->refs, n);
    w->seeded = 0;
    return 1;
}

/* ============================================================================
 *  SECTION 3 - Frame helpers and step-by-step display
 * ========================================================================== */

static void init_frames(int frames[], int nf)
{
    int i;
    for (i = 0; i < nf; i++)
        frames[i] = EMPTY_FRAME;
}

/* Return the index of 'page' in the frame table, or -1 if it is not loaded. */
static int find_page(const int frames[], int nf, int page)
{
    int i;
    for (i = 0; i < nf; i++)
        if (frames[i] == page)
            return i;
    return -1;
}

static void trace_header(const char *algo, const char *rule, int nf)
{
    int i;
    printf("\n=== %s ===\n", algo);
    printf("Rule: %s\n", rule);
    printf("%-5s %-5s | ", "Step", "Ref");
    for (i = 0; i < nf; i++)
        printf("F%-2d ", i + 1);
    printf("| %-6s | %s\n", "Result", "Evicted");
}

static void trace_step(int step, int page, const int frames[], int nf,
                       int hit, int evicted)
{
    int i;
    printf("%-5d %-5d | ", step, page);
    for (i = 0; i < nf; i++) {
        if (frames[i] == EMPTY_FRAME) printf("-   ");
        else                          printf("%-3d ", frames[i]);
    }
    printf("| %-6s | ", hit ? "HIT" : "FAULT");
    if (evicted == EMPTY_FRAME) printf("-\n");
    else                        printf("%d\n", evicted);
}

static void reset_result(Result *r, const char *name)
{
    memset(r, 0, sizeof *r);
    r->name = name;
}

/* Compute totals and ratios; guards against division by zero. */
static void finalize_result(Result *r)
{
    r->total = r->hits + r->faults;
    if (r->total > 0) {
        r->hit_ratio   = (double)r->hits   / (double)r->total;
        r->fault_ratio = (double)r->faults / (double)r->total;
        r->fault_pct   = r->fault_ratio * 100.0;
    } else {
        r->hit_ratio = r->fault_ratio = r->fault_pct = 0.0;
    }
}

static int frames_valid(int nf)
{
    return nf >= 1 && nf <= MAX_FRAMES;
}

/* ============================================================================
 *  SECTION 4 - The three page replacement algorithms
 *  Each has its own frame array, counters and replacement logic.
 * ========================================================================== */

/* FIFO: frames are filled in order 0..nf-1; afterwards a circular pointer
 * always points to the page that has been resident the longest. */
static Result run_fifo(const Workload *w, int nf, int verbose)
{
    int frames[MAX_FRAMES];
    int filled = 0, oldest = 0, i;
    Result r;

    reset_result(&r, "FIFO");
    if (!frames_valid(nf)) return r;
    init_frames(frames, nf);

    if (verbose)
        trace_header("FIFO (First-In, First-Out)",
                     "evict the page that has been in memory the longest", nf);

    for (i = 0; i < w->count; i++) {
        int page = w->refs[i];
        int hit = (find_page(frames, nf, page) >= 0);
        int evicted = EMPTY_FRAME;

        if (hit) {
            r.hits++;
        } else {
            r.faults++;
            if (filled < nf) {
                frames[filled++] = page;          /* free frame available */
            } else {
                evicted = frames[oldest];         /* replace the oldest   */
                frames[oldest] = page;
                oldest = (oldest + 1) % nf;
            }
        }
        if (verbose) trace_step(i + 1, page, frames, nf, hit, evicted);
    }
    finalize_result(&r);
    return r;
}

/* LRU: remember the time of last use of every loaded page and evict the
 * page with the smallest timestamp. A hit refreshes the timestamp. */
static Result run_lru(const Workload *w, int nf, int verbose)
{
    int frames[MAX_FRAMES];
    int last_used[MAX_FRAMES];
    int i, j;
    Result r;

    reset_result(&r, "LRU");
    if (!frames_valid(nf)) return r;
    init_frames(frames, nf);
    for (j = 0; j < nf; j++) last_used[j] = -1;

    if (verbose)
        trace_header("LRU (Least Recently Used)",
                     "evict the page that has not been used for the longest time", nf);

    for (i = 0; i < w->count; i++) {
        int page = w->refs[i];
        int pos = find_page(frames, nf, page);
        int hit = (pos >= 0);
        int evicted = EMPTY_FRAME;

        if (hit) {
            r.hits++;
            last_used[pos] = i;                   /* refresh recency */
        } else {
            int victim = find_page(frames, nf, EMPTY_FRAME);
            r.faults++;
            if (victim < 0) {                     /* memory full: pick LRU */
                victim = 0;
                for (j = 1; j < nf; j++)
                    if (last_used[j] < last_used[victim])
                        victim = j;
                evicted = frames[victim];
            }
            frames[victim] = page;
            last_used[victim] = i;
        }
        if (verbose) trace_step(i + 1, page, frames, nf, hit, evicted);
    }
    finalize_result(&r);
    return r;
}

/* Position of the next use of 'page' at or after index 'from', or NEVER_USED. */
static int next_use(const int refs[], int count, int from, int page)
{
    int j;
    for (j = from; j < count; j++)
        if (refs[j] == page)
            return j;
    return NEVER_USED;
}

/* Optimal (Belady's MIN): evict the page used farthest in the future. A page
 * that is never used again is the best victim. Ties (several pages never
 * used again) are broken by choosing the lowest frame number. */
static Result run_optimal(const Workload *w, int nf, int verbose)
{
    int frames[MAX_FRAMES];
    int i, j;
    Result r;

    reset_result(&r, "Optimal");
    if (!frames_valid(nf)) return r;
    init_frames(frames, nf);

    if (verbose)
        trace_header("Optimal (theoretical benchmark)",
                     "evict the page whose next use is farthest away (or never)", nf);

    for (i = 0; i < w->count; i++) {
        int page = w->refs[i];
        int hit = (find_page(frames, nf, page) >= 0);
        int evicted = EMPTY_FRAME;

        if (hit) {
            r.hits++;
        } else {
            int victim = find_page(frames, nf, EMPTY_FRAME);
            r.faults++;
            if (victim < 0) {                     /* memory full: look ahead */
                int farthest = -1;
                for (j = 0; j < nf; j++) {
                    int nu = next_use(w->refs, w->count, i + 1, frames[j]);
                    if (nu > farthest) {
                        farthest = nu;
                        victim = j;
                    }
                }
                evicted = frames[victim];
            }
            frames[victim] = page;
        }
        if (verbose) trace_step(i + 1, page, frames, nf, hit, evicted);
    }
    finalize_result(&r);
    return r;
}

/* ============================================================================
 *  SECTION 5 - Reporting: tables, comparison, interpretation
 * ========================================================================== */

static void print_rule(char ch, int n)
{
    int i;
    for (i = 0; i < n; i++) putchar(ch);
    putchar('\n');
}

static void print_reference_string(const Workload *w)
{
    int i;
    printf("  Reference string:");
    for (i = 0; i < w->count; i++) {
        if (i % 20 == 0) printf("\n    ");
        printf("%d ", w->refs[i]);
    }
    printf("\n");
}

/* Banner shown before every analysis (scenario, refs, distinct, frames). */
static void print_config(const Workload *w, int frames)
{
    printf("\n");
    print_rule('-', 72);
    printf("  Scenario         : %s\n", w->name);
    printf("  Real-world model : %s\n", w->scenario);
    printf("  Page references  : %d\n", w->count);
    printf("  Distinct pages   : %d\n", w->distinct);
    printf("  Memory frames    : %d\n", frames);
    if (w->seeded) printf("  Random seed      : %u\n", w->seed);
    print_reference_string(w);
    print_rule('-', 72);
}

static void print_results_table(const Result res[], int n)
{
    int i;
    printf("\n%-10s %6s %7s %8s %11s %12s %9s\n",
           "Algorithm", "Refs", "Hits", "Faults", "Hit ratio", "Fault ratio", "Fault %");
    print_rule('-', 70);
    for (i = 0; i < n; i++)
        printf("%-10s %6d %7d %8d %11.4f %12.4f %8.2f%%\n",
               res[i].name, res[i].total, res[i].hits, res[i].faults,
               res[i].hit_ratio, res[i].fault_ratio, res[i].fault_pct);
    print_rule('-', 70);
}

/* Write the names of all algorithms that reach the minimum fault count. */
static void list_lowest(const int faults[NUM_ALGOS], char *out, size_t size)
{
    int min = faults[0], i, first = 1;
    size_t used = 0;

    for (i = 1; i < NUM_ALGOS; i++)
        if (faults[i] < min) min = faults[i];

    out[0] = '\0';
    for (i = 0; i < NUM_ALGOS; i++) {
        if (faults[i] == min) {
            int n = snprintf(out + used, size - used, "%s%s",
                             first ? "" : ", ", ALGO_NAMES[i]);
            if (n < 0 || (size_t)n >= size - used) return;
            used += (size_t)n;
            first = 0;
        }
    }
}

static int min_faults(const int faults[NUM_ALGOS])
{
    int m = faults[0], i;
    for (i = 1; i < NUM_ALGOS; i++)
        if (faults[i] < m) m = faults[i];
    return m;
}

/* Sanity checks required by the specification. Returns 1 if all pass. */
static int verify_results(const Result res[NUM_ALGOS], int refs, int verbose)
{
    int ok = 1, i;

    for (i = 0; i < NUM_ALGOS; i++) {
        if (res[i].hits + res[i].faults != refs) {
            ok = 0;
            if (verbose) printf("  [FAIL] %s: hits + faults != references\n", res[i].name);
        }
    }
    if (res[2].faults > res[0].faults || res[2].faults > res[1].faults) {
        ok = 0;
        if (verbose) printf("  [FAIL] Optimal produced more faults than FIFO or LRU\n");
    }
    if (verbose && ok) {
        printf("  [OK] hits + faults == %d reference(s) for every algorithm\n", refs);
        printf("  [OK] Optimal faults (%d) <= FIFO (%d) and LRU (%d)\n",
               res[2].faults, res[0].faults, res[1].faults);
    }
    return ok;
}

static void print_comparison(const Result res[NUM_ALGOS], const Workload *w)
{
    int faults[NUM_ALGOS], i;
    char names[64];

    for (i = 0; i < NUM_ALGOS; i++) faults[i] = res[i].faults;
    list_lowest(faults, names, sizeof names);

    printf("\nLowest fault count: %s (%d fault(s))\n", names, min_faults(faults));
    printf("Fault differences : FIFO - LRU = %+d, FIFO - Optimal = %+d, LRU - Optimal = %+d\n",
           faults[0] - faults[1], faults[0] - faults[2], faults[1] - faults[2]);
    printf("Compulsory (first-touch) faults: %d  -> faults beyond this are caused by "
           "limited frames\n", w->distinct);
    printf("\nSelf-check:\n");
    verify_results(res, w->count, 1);
}

/* Data-driven interpretation of one comparison. Only numbers computed from
 * the run are quoted; the descriptive sentences explain the workload type. */
static void print_interpretation(const Workload *w, int frames, const Result res[NUM_ALGOS])
{
    int fifo = res[0].faults, lru = res[1].faults, opt = res[2].faults;
    int best_real = (fifo < lru) ? fifo : lru;
    double pct = (w->count > 0) ? 100.0 * best_real / w->count : 0.0;

    printf("\n--- Interpretation (real-world context) ---\n");

    switch (w->kind) {
    case WL_SEQUENTIAL:
        printf("  * A sequential scan (file read, video stream) walks through memory in order and\n"
               "    loops; a page is often reused only after many other pages have been touched.\n");
        break;
    case WL_REPEATED:
        printf("  * A repeated-access workload resembles a database or application that keeps\n"
               "    returning to a small working set of data pages, with occasional cold lookups.\n");
        break;
    case WL_RANDOM:
        printf("  * Random access resembles many unrelated applications or tabs: there is little\n"
               "    pattern for any policy to exploit.\n");
        break;
    case WL_LOCALITY:
        printf("  * Locality of reference approximates programs that keep revisiting recently used\n"
               "    memory while their working set slowly moves.\n");
        break;
    case WL_PRESSURE:
        printf("  * Memory pressure shows what happens when far more pages are in use than there\n"
               "    are frames: pages are evicted before they can be reused.\n");
        break;
    default:
        printf("  * Custom sequence: the interpretation below is based only on the numbers.\n");
        break;
    }

    if (frames >= w->distinct) {
        printf("  * Frames (%d) >= distinct pages (%d): every page fits in memory, so each algorithm\n"
               "    only suffers the %d compulsory faults; extra frames would not help.\n",
               frames, w->distinct, w->distinct);
    } else if (fifo == w->count && lru == w->count) {
        printf("  * FIFO and LRU faulted on every reference: pages were evicted just before they\n"
               "    were needed again (the classic cyclic-scan pattern with too few frames).\n");
    } else if (pct >= 75.0) {
        printf("  * Fault rate of the best practical policy is %.1f%%: very high. The %d frames are\n"
               "    far too few for this workload; a real system would be waiting on storage most\n"
               "    of the time (thrashing).\n", pct, frames);
    } else if (pct >= 40.0) {
        printf("  * Fault rate of the best practical policy is %.1f%%: substantial. More frames\n"
               "    would probably reduce faults noticeably (see the frame-capacity experiment).\n", pct);
    } else {
        printf("  * Fault rate of the best practical policy is %.1f%%: moderate/low. The %d frames\n"
               "    cover most of the active working set.\n", pct, frames);
    }

    if (frames < w->distinct) {
        if (lru < fifo) {
            printf("  * LRU had %d fewer faults than FIFO. Each access refreshes a page's recency, so\n"
                   "    frequently used pages stay resident, while FIFO evicts the oldest-loaded page\n"
                   "    even if it is used all the time.\n", fifo - lru);
        } else if (lru > fifo) {
            printf("  * FIFO had %d fewer faults than LRU on this input: recency was not a good\n"
                   "    predictor of reuse here (this is allowed - LRU is not always better).\n",
                   lru - fifo);
        } else {
            printf("  * FIFO and LRU tied at %d faults for this input.\n", fifo);
        }
        printf("  * Optimal needed %d faults; the best practical policy is %d faults above this\n"
               "    benchmark. Optimal is theoretical: it needs future knowledge that a real OS\n"
               "    does not have.\n", opt, best_real - opt);
    }

    printf("  * Note: this is a page-replacement simulator. Real systems also involve caching,\n"
           "    working-set tracking, storage latency and allocation policies that are not\n"
           "    modelled here.\n");
}

static void print_limitations(void)
{
    printf("\n=== Real-world interpretation and limitations ===\n");
    printf("What the workloads represent:\n");
    printf("  - Sequential scan   : file scans or media streaming.\n");
    printf("  - Repeated access   : a database or application re-reading a hot working set.\n");
    printf("  - Random access     : many unrelated tasks with no reuse pattern.\n");
    printf("  - Locality          : programs that revisit recently used memory.\n");
    printf("  - Memory pressure   : a device whose frames are far fewer than the pages in use.\n");
    printf("\nHow to read the results:\n");
    printf("  - A high fault rate suggests the frames are insufficient for the simulated workload.\n");
    printf("  - Optimal is a theoretical benchmark (it uses future knowledge); real OSes cannot do this.\n");
    printf("  - FIFO can show Belady's anomaly: more frames can occasionally cause MORE faults.\n");
    printf("  - LRU is not guaranteed to beat FIFO; the data decides.\n");
    printf("\nLimitations:\n");
    printf("  - This is a page-replacement SIMULATOR. It does NOT measure page faults in your running\n");
    printf("    operating system, does not inspect actual RAM and does not capture hardware faults.\n");
    printf("  - Real systems add caching, working-set behaviour, disk/SSD latency, prefetching,\n");
    printf("    dirty-page write-back and allocation policies that this model ignores.\n");
    printf("  - Workloads are synthetic models with fixed parameters, not recorded traces.\n");
}

/* ============================================================================
 *  SECTION 6 - Menu actions
 * ========================================================================== */

static void store_last(AppState *s, const Result res[], const int have[])
{
    int i;
    memset(&s->last, 0, sizeof s->last);
    s->last.valid = 1;
    snprintf(s->last.name, sizeof s->last.name, "%s", s->workload.name);
    s->last.refs = s->workload.count;
    s->last.distinct = s->workload.distinct;
    s->last.frames = s->frames;
    for (i = 0; i < NUM_ALGOS; i++) {
        s->last.have[i] = have[i];
        if (have[i]) s->last.res[i] = res[i];
    }
}

/* Menu 1: run FIFO, LRU and Optimal on the same reference string. */
static void action_run_all(AppState *s)
{
    Result res[NUM_ALGOS];
    int have[NUM_ALGOS] = {1, 1, 1};
    int show, i;

    print_config(&s->workload, s->frames);
    show = prompt_yes_no("Show step-by-step frame states? [Y/n]: ", 1);

    for (i = 0; i < NUM_ALGOS; i++)
        res[i] = ALGO_FUNCS[i](&s->workload, s->frames, show);

    printf("\n=== Comparison: %s | %d frames ===", s->workload.name, s->frames);
    print_results_table(res, NUM_ALGOS);
    print_comparison(res, &s->workload);
    print_interpretation(&s->workload, s->frames, res);
    store_last(s, res, have);
}

/* Menu 2: run one algorithm only. */
static void action_run_one(AppState *s)
{
    Result res[NUM_ALGOS];
    int have[NUM_ALGOS] = {0, 0, 0};
    int choice, show;

    printf("\n  1) FIFO\n  2) LRU\n  3) Optimal\n");
    choice = (int)prompt_long("Choose algorithm (1-3, 0 to cancel): ", 0, 3, 0, 0);
    if (choice == 0) return;

    print_config(&s->workload, s->frames);
    show = prompt_yes_no("Show step-by-step frame states? [Y/n]: ", 1);

    memset(res, 0, sizeof res);
    res[choice - 1] = ALGO_FUNCS[choice - 1](&s->workload, s->frames, show);
    have[choice - 1] = 1;

    printf("\n=== %s result ===", ALGO_NAMES[choice - 1]);
    print_results_table(&res[choice - 1], 1);
    printf("Compulsory (first-touch) faults: %d\n", s->workload.distinct);
    if (res[choice - 1].hits + res[choice - 1].faults == s->workload.count)
        printf("[OK] hits + faults == %d reference(s)\n", s->workload.count);
    store_last(s, res, have);
}

/* Menu 3: choose a predefined real-world workload. */
static void action_select_scenario(AppState *s)
{
    int choice, count;
    long seed = (long)s->seed;
    char prompt[128];
    Workload w;

    printf("\nReal-world workload scenarios:\n");
    printf("  1) Sequential scan        - file scan / media streaming (loops over %d pages)\n", SEQ_PAGES);
    printf("  2) Repeated access        - database: %d%% of requests hit %d hot pages (+%d cold)\n",
           HOT_PERCENT, HOT_PAGES, COLD_PAGES);
    printf("  3) Random access          - many unrelated apps/tabs (uniform over %d pages)\n", RANDOM_POOL);
    printf("  4) Locality of reference  - desktop multitasking (%d-page working set, %d%% reuse)\n",
           LOC_WINDOW, LOC_STAY_PERCENT);
    printf("  5) Memory pressure        - distinct pages = %d x frames\n", PRESSURE_FACTOR);
    choice = (int)prompt_long("Choose scenario (1-5, 0 to cancel): ", 0, NUM_SCENARIOS, 0, 0);
    if (choice == 0) return;

    snprintf(prompt, sizeof prompt, "Number of page references (1-%d, Enter = %d): ",
             MAX_REFS, s->gen_refs);
    count = (int)prompt_long(prompt, 1, MAX_REFS, s->gen_refs, 1);

    if (choice != 1) {
        snprintf(prompt, sizeof prompt, "Random seed (1-%ld, Enter = %u): ", MAX_SEED, s->seed);
        seed = prompt_long(prompt, 1, MAX_SEED, (long)s->seed, 1);
    }

    generate_workload(&w, (WorkloadKind)(choice - 1), count, (unsigned int)seed, s->frames);
    s->workload = w;
    s->gen_refs = count;
    s->seed = (unsigned int)seed;

    printf("\nScenario selected.");
    print_config(&s->workload, s->frames);
}

/* Menu 4: enter a custom reference string. */
static void action_custom_sequence(AppState *s)
{
    char line[LINE_BUF];
    char err[160];
    Workload w;

    printf("\nEnter page numbers (0-%d) separated by spaces or commas, up to %d references.\n",
           MAX_PAGE_NUM, MAX_REFS);
    printf("Example: 7 0 1 2 0 3 0 4 2 3 0 3 2\n");
    for (;;) {
        printf("Sequence (empty line to cancel): ");
        fflush(stdout);
        if (!read_line(line, sizeof line)) {
            printf("  Invalid input: line too long (maximum %d characters).\n", LINE_BUF - 1);
            continue;
        }
        if (is_blank(line)) {
            printf("  Cancelled - previous workload kept.\n");
            return;
        }
        if (parse_sequence(line, &w, err, sizeof err)) break;
        printf("  Invalid input: %s\n", err);
    }
    s->workload = w;
    printf("\nCustom sequence accepted.");
    print_config(&s->workload, s->frames);
}

/* Menu 5: change the number of frames. */
static void action_set_frames(AppState *s)
{
    char prompt[96];
    int old = s->frames;

    snprintf(prompt, sizeof prompt, "Number of memory frames (1-%d, Enter = %d): ",
             MAX_FRAMES, s->frames);
    s->frames = (int)prompt_long(prompt, 1, MAX_FRAMES, s->frames, 1);
    printf("Frames set to %d.\n", s->frames);

    /* The memory-pressure workload depends on the frame count: rebuild it. */
    if (s->workload.kind == WL_PRESSURE && old != s->frames) {
        Workload w;
        generate_workload(&w, WL_PRESSURE, s->workload.count, s->workload.seed, s->frames);
        s->workload = w;
        printf("Memory-pressure workload regenerated for %d frames (%d distinct pages).\n",
               s->frames, s->workload.distinct);
    }
}

/* Menu 6: same workload, different frame capacities. */
static void action_frame_experiment(AppState *s)
{
    int faults[MAX_FRAMES + 1][NUM_ALGOS];
    int max_f, f, i, ok = 1;
    int lru_better = 0, fifo_better = 0, tied = 0;
    int anomalies = 0;
    char prompt[96], names[64];

    print_config(&s->workload, s->frames);
    snprintf(prompt, sizeof prompt,
             "Test frame capacities 1..N. N (1-%d, Enter = 5): ", MAX_FRAMES);
    max_f = (int)prompt_long(prompt, 1, MAX_FRAMES, 5, 1);

    for (f = 1; f <= max_f; f++)
        for (i = 0; i < NUM_ALGOS; i++)
            faults[f][i] = ALGO_FUNCS[i](&s->workload, f, 0).faults;

    printf("\n=== Frame-capacity experiment: %s (%d references, %d distinct pages) ===\n",
           s->workload.name, s->workload.count, s->workload.distinct);
    printf("Cells show faults (change versus the previous capacity).\n\n");
    printf("%-7s %-13s %-13s %-13s %-9s %-9s %s\n",
           "Frames", "FIFO", "LRU", "Optimal", "FIFO-Opt", "LRU-Opt", "Lowest");
    print_rule('-', 84);

    for (f = 1; f <= max_f; f++) {
        char cell[NUM_ALGOS][16];
        for (i = 0; i < NUM_ALGOS; i++) {
            if (f == 1)
                snprintf(cell[i], sizeof cell[i], "%d", faults[f][i]);
            else
                snprintf(cell[i], sizeof cell[i], "%d (%+d)", faults[f][i],
                         faults[f][i] - faults[f - 1][i]);
            if (f > 1 && faults[f][i] > faults[f - 1][i]) anomalies++;
        }
        list_lowest(faults[f], names, sizeof names);
        printf("%-7d %-13s %-13s %-13s %-9d %-9d %s\n", f, cell[0], cell[1], cell[2],
               faults[f][0] - faults[f][2], faults[f][1] - faults[f][2], names);

        if (faults[f][2] > faults[f][0] || faults[f][2] > faults[f][1]) ok = 0;
        if (faults[f][1] < faults[f][0]) lru_better++;
        else if (faults[f][1] > faults[f][0]) fifo_better++;
        else tied++;
    }
    print_rule('-', 84);

    printf("\nObservations (computed from the table):\n");
    printf("  * LRU had fewer faults than FIFO at %d capacity(ies), FIFO fewer at %d, tied at %d.\n",
           lru_better, fifo_better, tied);
    if (anomalies > 0) {
        printf("  * Fault count INCREASED when a frame was added (%d time(s)):\n", anomalies);
        for (f = 2; f <= max_f; f++)
            for (i = 0; i < NUM_ALGOS; i++)
                if (faults[f][i] > faults[f - 1][i])
                    printf("      %s: %d frames -> %d faults, %d frames -> %d faults%s\n",
                           ALGO_NAMES[i], f - 1, faults[f - 1][i], f, faults[f][i],
                           (i == 0) ? "  (Belady's anomaly)" : "  (unexpected)");
    } else {
        printf("  * Adding frames never increased the fault count for any algorithm here.\n");
    }
    printf("  * Compulsory faults (distinct pages) = %d: no algorithm can go below this.\n",
           s->workload.distinct);
    printf("  * [%s] Optimal <= FIFO and LRU at every tested capacity\n", ok ? "OK" : "FAIL");
    printf("  * Optimal is a theoretical benchmark: it requires knowing future references.\n");
}

/* Menu 7: run all workloads under the same frame count and compare. */
static void action_workload_comparison(AppState *s)
{
    static const char *short_names[NUM_SCENARIOS] =
        { "Sequential", "Repeated", "Random", "Locality", "Pressure" };
    int frames = s->frames, count, k, i;
    long seed;
    char prompt[128], names[64];
    Workload w;
    Result res[NUM_SCENARIOS][NUM_ALGOS];
    Workload wl[NUM_SCENARIOS];
    int all_ok = 1;

    snprintf(prompt, sizeof prompt, "Number of memory frames (1-%d, Enter = %d): ", MAX_FRAMES, s->frames);
    frames = (int)prompt_long(prompt, 1, MAX_FRAMES, s->frames, 1);
    snprintf(prompt, sizeof prompt, "Page references per workload (1-%d, Enter = %d): ", MAX_REFS, s->gen_refs);
    count = (int)prompt_long(prompt, 1, MAX_REFS, s->gen_refs, 1);
    snprintf(prompt, sizeof prompt, "Random seed (1-%ld, Enter = %u): ", MAX_SEED, s->seed);
    seed = prompt_long(prompt, 1, MAX_SEED, (long)s->seed, 1);

    printf("\n=== Overall workload comparison: %d frames, %d references each, seed %ld ===\n",
           frames, count, seed);

    for (k = 0; k < NUM_SCENARIOS; k++) {
        generate_workload(&w, (WorkloadKind)k, count, (unsigned int)seed, frames);
        wl[k] = w;
        for (i = 0; i < NUM_ALGOS; i++)
            res[k][i] = ALGO_FUNCS[i](&w, frames, 0);
        if (!verify_results(res[k], w.count, 0)) all_ok = 0;

        printf("\nWorkload: %s\n  Models : %s\n  Refs=%d  Distinct=%d  Frames=%d\n",
               w.name, w.scenario, w.count, w.distinct, frames);
        print_results_table(res[k], NUM_ALGOS);
        {
            int f3[NUM_ALGOS];
            for (i = 0; i < NUM_ALGOS; i++) f3[i] = res[k][i].faults;
            list_lowest(f3, names, sizeof names);
            printf("Lowest fault count: %s (%d fault(s))\n", names, min_faults(f3));
        }
    }

    printf("\n=== Summary of page faults ===\n");
    printf("%-11s %6s %8s %7s %6s %8s  %-20s %s\n",
           "Workload", "Refs", "Distinct", "FIFO", "LRU", "Optimal", "Lowest", "FIFO vs LRU");
    print_rule('-', 86);
    for (k = 0; k < NUM_SCENARIOS; k++) {
        int f3[NUM_ALGOS];
        const char *cmp;
        for (i = 0; i < NUM_ALGOS; i++) f3[i] = res[k][i].faults;
        list_lowest(f3, names, sizeof names);
        if (f3[1] < f3[0]) cmp = "LRU better";
        else if (f3[1] > f3[0]) cmp = "FIFO better";
        else cmp = "tie";
        printf("%-11s %6d %8d %7d %6d %8d  %-20s %s\n", short_names[k], wl[k].count,
               wl[k].distinct, f3[0], f3[1], f3[2], names, cmp);
    }
    print_rule('-', 86);
    printf("[%s] hits + faults == references and Optimal <= FIFO, LRU for every workload\n",
           all_ok ? "OK" : "FAIL");
    printf("Optimal is a theoretical benchmark; compare FIFO and LRU for realistic guidance.\n");
    printf("This is a simulation of synthetic workloads, not a measurement of a real system.\n");
}

/* Menu 8: show the metrics of the last analysis. */
static void action_view_metrics(const AppState *s)
{
    const LastRun *L = &s->last;
    int i, opt_have;

    if (!L->valid) {
        printf("\nNo analysis has been run yet. Use option 1 or 2 first.\n");
        return;
    }
    printf("\n=== Performance metrics of the last analysis ===\n");
    printf("  Scenario: %s | References: %d | Distinct pages: %d | Frames: %d\n",
           L->name, L->refs, L->distinct, L->frames);

    printf("\n%-10s %6s %7s %8s %11s %12s %9s\n",
           "Algorithm", "Refs", "Hits", "Faults", "Hit ratio", "Fault ratio", "Fault %");
    print_rule('-', 70);
    for (i = 0; i < NUM_ALGOS; i++)
        if (L->have[i])
            printf("%-10s %6d %7d %8d %11.4f %12.4f %8.2f%%\n",
                   L->res[i].name, L->res[i].total, L->res[i].hits, L->res[i].faults,
                   L->res[i].hit_ratio, L->res[i].fault_ratio, L->res[i].fault_pct);
    print_rule('-', 70);

    opt_have = L->have[2];
    printf("\n%-10s %12s %20s %18s %12s\n",
           "Algorithm", "Compulsory", "Replacement-caused", "Gap vs Optimal", "Efficiency");
    print_rule('-', 78);
    for (i = 0; i < NUM_ALGOS; i++) {
        if (!L->have[i]) continue;
        printf("%-10s %12d %20d ", L->res[i].name, L->distinct, L->res[i].faults - L->distinct);
        if (opt_have) {
            int gap = L->res[i].faults - L->res[2].faults;
            if (L->res[i].faults > 0)
                printf("%18d %11.1f%%\n", gap, 100.0 * L->res[2].faults / L->res[i].faults);
            else
                printf("%18d %12s\n", gap, "n/a");
        } else {
            printf("%18s %12s\n", "(run all)", "n/a");
        }
    }
    print_rule('-', 78);
    printf("Efficiency = Optimal faults / this algorithm's faults (100%% = matches the benchmark).\n");
}

/* Menu 10: built-in self-test, also reachable with --selftest. */
typedef struct {
    const char *name;
    int refs[24];
    int n;
    int frames;
    int expected[NUM_ALGOS];  /* FIFO, LRU, Optimal - well-known textbook values */
} KnownCase;

static int check(int cond, const char *label, int *passed, int *failed)
{
    if (cond) { (*passed)++; printf("  [PASS] %s\n", label); }
    else      { (*failed)++; printf("  [FAIL] %s\n", label); }
    return cond;
}

static int run_selftest(void)
{
    static const KnownCase cases[] = {
        { "Textbook string, 3 frames", {7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1}, 20, 3, {15, 12, 9} },
        { "Belady string, 3 frames",   {1,2,3,4,1,2,5,1,2,3,4,5},                  12, 3, {9, 10, 7} },
        { "Belady string, 4 frames",   {1,2,3,4,1,2,5,1,2,3,4,5},                  12, 4, {10, 8, 6} },
        { "Single reference",          {5},                                         1, 1, {1, 1, 1} },
        { "Same page repeated",        {3,3,3,3},                                   4, 2, {1, 1, 1} },
        { "Single frame",              {1,2,1,2},                                   4, 1, {4, 4, 4} },
        { "More frames than pages",    {1,2,3,1,2,3},                               6, 5, {3, 3, 3} },
    };
    int passed = 0, failed = 0, c, k, f, i;
    char label[128];
    Workload w, w2;

    printf("\n=== Built-in self-test ===\n");
    printf("Known cases (expected values are standard textbook results):\n");
    for (c = 0; c < (int)(sizeof cases / sizeof cases[0]); c++) {
        Result r[NUM_ALGOS];
        memset(&w, 0, sizeof w);
        memcpy(w.refs, cases[c].refs, (size_t)cases[c].n * sizeof(int));
        w.count = cases[c].n;
        w.distinct = count_distinct(w.refs, w.count);
        for (i = 0; i < NUM_ALGOS; i++) r[i] = ALGO_FUNCS[i](&w, cases[c].frames, 0);
        snprintf(label, sizeof label, "%s: FIFO=%d LRU=%d Optimal=%d (expected %d/%d/%d)",
                 cases[c].name, r[0].faults, r[1].faults, r[2].faults,
                 cases[c].expected[0], cases[c].expected[1], cases[c].expected[2]);
        check(r[0].faults == cases[c].expected[0] && r[1].faults == cases[c].expected[1] &&
              r[2].faults == cases[c].expected[2], label, &passed, &failed);
    }

    printf("\nInvariants on every generated workload (frames 1..%d, 3 seeds):\n", MAX_FRAMES);
    {
        int sum_ok = 1, opt_ok = 1, rep_ok = 1;
        static const unsigned int seeds[3] = { 1u, 42u, 2024u };
        for (k = 0; k < NUM_SCENARIOS; k++) {
            for (f = 1; f <= MAX_FRAMES; f++) {
                int s;
                for (s = 0; s < 3; s++) {
                    Result r[NUM_ALGOS];
                    generate_workload(&w, (WorkloadKind)k, MAX_REFS, seeds[s], f);
                    generate_workload(&w2, (WorkloadKind)k, MAX_REFS, seeds[s], f);
                    if (memcmp(w.refs, w2.refs, sizeof w.refs) != 0) rep_ok = 0;
                    for (i = 0; i < NUM_ALGOS; i++) {
                        r[i] = ALGO_FUNCS[i](&w, f, 0);
                        if (r[i].hits + r[i].faults != w.count) sum_ok = 0;
                        if (r[i].faults < w.distinct) sum_ok = 0; /* cannot beat compulsory faults */
                    }
                    if (r[2].faults > r[0].faults || r[2].faults > r[1].faults) opt_ok = 0;
                }
            }
        }
        check(sum_ok, "hits + faults == references, and faults >= distinct pages", &passed, &failed);
        check(opt_ok, "Optimal faults <= FIFO and LRU", &passed, &failed);
        check(rep_ok, "same seed -> identical workload (reproducible)", &passed, &failed);
    }

    printf("\nInput validation:\n");
    {
        char err[160];
        check(!parse_sequence("1 2 x 4", &w, err, sizeof err), "rejects non-numeric token", &passed, &failed);
        check(!parse_sequence("1 2 100", &w, err, sizeof err), "rejects page number above limit", &passed, &failed);
        check(!parse_sequence("1 -2 3", &w, err, sizeof err), "rejects negative page number", &passed, &failed);
        check(!parse_sequence("   ", &w, err, sizeof err), "rejects empty sequence", &passed, &failed);
        check(parse_sequence("1, 2,3  4", &w, err, sizeof err) && w.count == 4, "accepts spaces and commas", &passed, &failed);
    }

    printf("\nSelf-test summary: %d passed, %d failed\n", passed, failed);
    return failed;
}

/* ============================================================================
 *  SECTION 7 - Welcome screen, menu, main
 * ========================================================================== */

static void print_welcome(void)
{
    printf("\n");
    print_rule('=', 72);
    printf("                       PAGE FAULT ANALYZER\n");
    printf("        Virtual-memory page replacement simulation in C\n");
    printf("              FIFO  |  LRU  |  Optimal comparison\n");
    print_rule('=', 72);
    printf("NOTE: this is a page-replacement SIMULATOR. It does not measure page\n");
    printf("faults of your running operating system or inspect actual RAM.\n");
}

static void print_menu(const AppState *s)
{
    printf("\n");
    print_rule('=', 72);
    printf(" Current: %s | refs=%d | distinct=%d | frames=%d\n",
           s->workload.name, s->workload.count, s->workload.distinct, s->frames);
    print_rule('-', 72);
    printf("  1  Run all algorithms (FIFO, LRU, Optimal)\n");
    printf("  2  Run an individual algorithm\n");
    printf("  3  Select a real-world workload scenario\n");
    printf("  4  Enter a custom page-reference sequence\n");
    printf("  5  Set number of memory frames\n");
    printf("  6  Compare different frame capacities (experiment)\n");
    printf("  7  Compare all workloads (overall report)\n");
    printf("  8  View performance metrics (last analysis)\n");
    printf("  9  Real-world interpretation and limitations\n");
    printf(" 10  Run built-in self-test\n");
    printf("  0  Exit\n");
    print_rule('=', 72);
}

int main(int argc, char *argv[])
{
    AppState state;

    if (argc > 1) {
        if (strcmp(argv[1], "--selftest") == 0)
            return run_selftest() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        printf("Usage: %s [--selftest]\n", argv[0]);
        return EXIT_FAILURE;
    }

    memset(&state, 0, sizeof state);
    state.frames = DEFAULT_FRAMES;
    state.gen_refs = DEFAULT_REFS;
    state.seed = DEFAULT_SEED;
    generate_workload(&state.workload, WL_LOCALITY, state.gen_refs, state.seed, state.frames);

    print_welcome();

    for (;;) {
        int choice;
        print_menu(&state);
        choice = (int)prompt_long("Enter your choice (0-10): ", 0, 10, 0, 0);
        switch (choice) {
        case 1:  action_run_all(&state);            break;
        case 2:  action_run_one(&state);            break;
        case 3:  action_select_scenario(&state);    break;
        case 4:  action_custom_sequence(&state);    break;
        case 5:  action_set_frames(&state);         break;
        case 6:  action_frame_experiment(&state);   break;
        case 7:  action_workload_comparison(&state);break;
        case 8:  action_view_metrics(&state);       break;
        case 9:  print_limitations();               break;
        case 10: run_selftest();                    break;
        case 0:
            printf("\nThank you for using Page Fault Analyzer. Goodbye!\n");
            return EXIT_SUCCESS;
        default:
            break; /* unreachable: prompt_long validates the range */
        }
    }
}
