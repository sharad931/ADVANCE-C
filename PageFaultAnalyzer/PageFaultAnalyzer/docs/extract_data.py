"""Extract result tables from the real program outputs (tests/output_*.txt) into JSON for the report."""
import re, json
def lines(n): return open(f"tests/output_{n}.txt").read().split("\n")
ALG = re.compile(r"^(FIFO|LRU|Optimal)\s+(\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)%")
def comparison(n, header_regex, which=0):
    L = lines(n); idx = [i for i,l in enumerate(L) if re.search(header_regex, l)][which]
    rows = []; i = idx
    while len(rows) < 3:
        i += 1; m = ALG.match(L[i])
        if m: rows.append(dict(alg=m[1], refs=int(m[2]), hits=int(m[3]), faults=int(m[4]), hr=m[5], fr=m[6], pct=m[7]))
    low = next(l for l in L[i:] if l.startswith("Lowest fault count"))
    return dict(title=L[idx].strip("= "), rows=rows, lowest=low.split(":")[1].strip())
def experiment(n):
    L = lines(n); s = next(i for i,l in enumerate(L) if "Frame-capacity" in l)
    out = []
    for l in L[s:]:
        m = re.match(r"^(\d+)\s{2,}(\d+(?: \([+-]\d+\))?)\s+(\d+(?: \([+-]\d+\))?)\s+(\d+(?: \([+-]\d+\))?)\s+(\d+)\s+(\d+)\s+(.+)$", l)
        if m: out.append(list(m.groups()))
        if l.startswith("Observations"): break
    obs = []
    on = False
    for l in L[s:]:
        if l.startswith("Observations"): on = True; continue
        if on and l.startswith("  *"): obs.append(l.strip()[2:].strip())
        if on and l.strip() == "": break
    return dict(rows=out, obs=obs, title=L[s].strip("= "))
def summary(n):
    L = lines(n); s = next(i for i,l in enumerate(L) if "Summary of page faults" in l)
    out = []
    for l in L[s+3:]:
        m = re.match(r"^(\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(.+?)\s{2,}(.+)$", l)
        if m: out.append(list(m.groups()))
        elif l.startswith("---"): break
    return out
def blocks(n):
    L = lines(n); res = []
    for i,l in enumerate(L):
        if l.startswith("Workload: "):
            name = l[10:]; refs = L[i+2].strip()
            rows = []
            j = i
            while len(rows) < 3:
                j += 1; m = ALG.match(L[j])
                if m: rows.append([m[1], m[3], m[4], m[6]])
            lowest = next(x for x in L[j:] if x.startswith("Lowest")).split(":")[1].strip()
            res.append(dict(name=name, info=refs, rows=rows, lowest=lowest))
    return res
def metrics(n):
    L = lines(n); s = next(i for i,l in enumerate(L) if "Performance metrics of the last" in l)
    out = []
    for l in L[s:]:
        m = re.match(r"^(FIFO|LRU|Optimal)\s+(\d+)\s+(\d+)\s+(-?\d+)\s+([\d.]+%)", l)
        if m: out.append(list(m.groups()))
    return out
d = dict(
  textbook=comparison("01_custom_run_all", r"^=== Comparison"),
  textbook_metrics=metrics("01_custom_run_all"),
  belady=experiment("02_belady_experiment"),
  db=comparison("03_database_scenario", r"^=== Comparison"),
  db_exp=experiment("03_database_scenario"),
  overall=summary("04_workload_comparison"), overall_blocks=blocks("04_workload_comparison"),
  locality=comparison("07_other_scenarios", r"^=== Comparison: Locality"),
  sequential=comparison("07_other_scenarios", r"^=== Comparison: Sequential"),
  pressure=comparison("08_memory_pressure", r"^=== Comparison"),
  pressure_exp=experiment("08_memory_pressure"),
  selftest=[l.strip() for l in lines("00_selftest") if "[PASS]" in l or "[FAIL]" in l],
  selftest_summary=[l for l in lines("00_selftest") if l.startswith("Self-test summary")][0],
)
json.dump(d, open("docs/report_data.json","w"), indent=1)
print({k:(len(v) if hasattr(v,'__len__') else v) for k,v in d.items()})
print(d["overall"]); print(d["textbook_metrics"]); print(d["belady"]["rows"][:2])
