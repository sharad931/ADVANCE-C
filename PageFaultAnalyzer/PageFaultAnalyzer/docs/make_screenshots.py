"""Render real program output (tests/output_*.txt) as terminal-style PNG images."""
import re, sys
from PIL import Image, ImageDraw, ImageFont
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
def render(lines, path, title):
    f = ImageFont.truetype(FONT, 15); fb = ImageFont.truetype(BOLD, 14)
    cw = f.getlength("M"); lh = 20; pad = 16; top = 34
    w = int(max(len(l) for l in lines) * cw + pad * 2 + 4); w = max(w, 640)
    h = top + lh * len(lines) + pad
    img = Image.new("RGB", (w, h), (24, 26, 31)); d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, 28], fill=(48, 51, 59))
    for i, c in enumerate([(255, 95, 86), (255, 189, 46), (39, 201, 63)]):
        d.ellipse([10 + i * 20, 8, 22 + i * 20, 20], fill=c)
    d.text((w // 2 - 120, 6), title, font=fb, fill=(200, 200, 205))
    for i, l in enumerate(lines):
        col = (220, 222, 228)
        if "FAULT" in l and "|" in l: col = (255, 140, 130)
        elif " HIT " in l and "|" in l: col = (130, 220, 150)
        elif l.startswith("===") or "[OK]" in l or "[PASS]" in l: col = (120, 200, 255)
        elif "Invalid" in l or "[FAIL]" in l: col = (255, 170, 90)
        d.text((pad, top + i * lh), l, font=f, fill=col)
    img.save(path)
def read(n): return open(f"tests/output_{n}.txt").read().split("\n")
def between(lines, start, end=None, maxn=None, offset=0):
    s = next(i for i, l in enumerate(lines) if re.search(start, l)) + offset
    e = len(lines)
    if end: e = next(i for i in range(s + 1, len(lines)) if re.search(end, lines[i]))
    out = lines[s:e]
    return out[:maxn] if maxn else out
shots = []
L = read("01_custom_run_all")
shots.append(("01_welcome_and_menu.png", L[:26], "page_fault_analyzer - welcome screen"))
shots.append(("02_step_by_step_fifo_lru.png",
    between(L, r"^=== FIFO", r"^=== Optimal") , "Step-by-step frame states (FIFO then LRU)"))
shots.append(("03_comparison_table_textbook.png",
    between(L, r"^=== Comparison", r"^--- Interpretation") , "Comparison of FIFO, LRU, Optimal"))
L3 = read("03_database_scenario")
shots.append(("04_database_scenario_interpretation.png",
    L3[max(i for i,l in enumerate(L3) if l.startswith("  Scenario") and i < next(j for j,m in enumerate(L3) if m.startswith("=== Comparison")))-1:][:11] + [""] +
    between(L3, r"^=== Comparison", r"^=====+$"), "Scenario: database hot set, 4 frames"))
shots.append(("05_frame_capacity_experiment_db.png",
    between(L3, r"Frame-capacity", r"^=====+$"), "Frame-capacity experiment"))
L2 = read("02_belady_experiment")
shots.append(("06_belady_anomaly.png", between(L2, r"Frame-capacity", r"^=====+$"), "Belady's anomaly observed (FIFO)"))
L4 = read("04_workload_comparison")
shots.append(("07_workload_comparison.png", between(L4, r"Summary of page faults", r"^=====+$"), "Overall workload comparison"))
L7 = read("07_other_scenarios")
shots.append(("08_sequential_scan_scenario.png",
    between(L7, r"^=== Comparison: Sequential", r"^=====+$"), "Scenario: sequential scan"))
L0 = read("00_selftest")
shots.append(("09_self_test.png", [l for l in L0 if l.strip() != "" or True][:40], "page_fault_analyzer --selftest"))
L5 = read("05_invalid_inputs")
inv = [l for l in L5 if "Invalid" in l or "Cancelled" in l or "Please" in l or "Frames set" in l][:14]
shots.append(("10_input_validation.png", inv, "Input validation"))
for name, lines, title in shots:
    lines = [l.rstrip() for l in lines][:70]
    render(lines, f"screenshots/{name}", title); print(name, len(lines))
