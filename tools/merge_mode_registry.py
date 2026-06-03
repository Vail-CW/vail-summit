#!/usr/bin/env python3
"""
One-shot helper: parse the three legacy data tables in src/core/mode_registry.h
(parentTable, flagTable, trainingTable) and emit a single merged ModeInfo table.

Deterministic merge so we don't hand-transcribe ~84 rows. Prints the C++ table
to stdout plus an anomaly report (modes that appear in flag/training but not in
parent, so we can confirm their placement).
"""
import re
import sys
from collections import OrderedDict

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

REG = sys.argv[1] if len(sys.argv) > 1 else "src/core/mode_registry.h"

with open(REG, "r", encoding="utf-8") as f:
    text = f.read()

def slice_table(name):
    # Grab from "static const <Struct> <name>[] = {" to the matching "};"
    m = re.search(r"%s\[\]\s*=\s*\{" % re.escape(name), text)
    if not m:
        raise SystemExit("table not found: " + name)
    start = m.end()
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return text[start:i-1]

def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    s = re.sub(r"//[^\n]*", "", s)
    return s

# --- parentTable: { MODE_X, MODE_Y }, preserve order + section comments ---
parent_body = slice_table("parentTable")

# Capture order of modes and their parent, while remembering preceding comment
# lines so the merged table keeps the same human-readable section headers.
parent = OrderedDict()
order = []  # list of ("comment", text) or ("row", mode)
for raw_line in parent_body.splitlines():
    line = raw_line.strip()
    if not line:
        order.append(("blank", ""))
        continue
    if line.startswith("//"):
        order.append(("comment", line))
        continue
    m = re.match(r"\{\s*(MODE_\w+)\s*,\s*(MODE_\w+)\s*\}", line)
    if m:
        mode, par = m.group(1), m.group(2)
        parent[mode] = par
        order.append(("row", mode))
    # ignore trailing-comment-only or other lines

# --- flagTable: { MODE_X, <flag expr> } ---
flag_body = strip_comments(slice_table("flagTable"))
flags = {}
for m in re.finditer(r"\{\s*(MODE_\w+)\s*,\s*([^}]+?)\s*\}", flag_body):
    flags[m.group(1)] = re.sub(r"\s+", " ", m.group(2).strip())

# --- trainingTable: { MODE_X, "Name" } ---
train_body = strip_comments(slice_table("trainingTable"))
training = {}
for m in re.finditer(r'\{\s*(MODE_\w+)\s*,\s*"([^"]*)"\s*\}', train_body):
    training[m.group(1)] = m.group(2)

# --- anomalies: flag/training modes missing from parentTable ---
missing_parent = sorted(
    (set(flags) | set(training)) - set(parent)
)

# --- emit merged table, preserving parent order/sections ---
out = []
out.append("static const ModeInfo modeInfoTable[] = {")
out.append("    // mode, parent, flags, trainingName")
seen = set()
for kind, val in order:
    if kind == "comment":
        out.append("    " + val)
    elif kind == "blank":
        out.append("")
    elif kind == "row":
        mode = val
        seen.add(mode)
        par = parent[mode]
        fl = flags.get(mode, "0")
        tn = ('"%s"' % training[mode]) if mode in training else "nullptr"
        out.append("    { %s, %s, %s, %s }," % (mode, par, fl, tn))

# append flag/training-only modes (no parent entry) at the end
if missing_parent:
    out.append("")
    out.append("    // Modes that had flags/training but no explicit parent (parent defaults to main menu)")
    for mode in missing_parent:
        par = "MODE_MAIN_MENU"
        fl = flags.get(mode, "0")
        tn = ('"%s"' % training[mode]) if mode in training else "nullptr"
        out.append("    { %s, %s, %s, %s }," % (mode, par, fl, tn))

out.append("};")

print("\n".join(out))
print("\n// ==== ANOMALY REPORT ====", file=sys.stderr)
print("// parent rows: %d, flag rows: %d, training rows: %d" %
      (len(parent), len(flags), len(training)), file=sys.stderr)
print("// flag/training modes missing from parentTable: %s" %
      (", ".join(missing_parent) if missing_parent else "(none)"), file=sys.stderr)
