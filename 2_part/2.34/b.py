#!/usr/bin/env python3
"""Сборка и замер list.c для разных замков и длин списка.
Запуск:  python3 bench.py [секунд=5]
Результат: results.csv и results.md (markdown-таблица)."""
import os, re, subprocess, sys

SECS = int(sys.argv[1]) if len(sys.argv) > 1 else 5
LENS = [100, 1000, 10000, 100000]

# имя в таблице -> макрос сборки (4 способа; для остальных раскомментируй)
VARIANTS = {
    #"pthread_mutex": "USE_MUTEX",
    #"cas_spin":      "USE_CAS_SPIN",
    #"futex_mutex":   "USE_FUTEX_MUTEX",
    #"pthread_spin":  "USE_SPIN",
    "pthread_rwlock": "USE_RWLOCK",
}

here = os.path.dirname(os.path.abspath(__file__))
build = os.path.join(here, "build")
os.makedirs(build, exist_ok=True)

# FINAL: asc : i:75733 p:509 | desc: i:75889 p:476 | eq  : i:75705 p:14 | swaps=(55630 55835 56319)
FINAL_RE = re.compile(
    r"FINAL:\s*asc\s*:\s*i:(\d+)\s+p:\d+\s*\|\s*desc:\s*i:(\d+)\s+p:\d+\s*\|\s*"
    r"eq\s*:\s*i:(\d+)\s+p:\d+\s*\|\s*swaps=\((\d+) (\d+) (\d+)\)")

rows = []
for name, macro in VARIANTS.items():
    exe = os.path.join(build, name)
    subprocess.run(["gcc", "-O2", "-Wall", "-pthread", "-D" + macro,
                    os.path.join(here, "list.c"), "-o", exe], check=True)
    for n in LENS:
        print(f"{name:15} len={n:<7} ...", flush=True)
        out = subprocess.run([exe, str(n), str(SECS)], capture_output=True,
                             text=True, check=True).stdout
        m = FINAL_RE.search(out)
        if not m:
            sys.exit("не удалось разобрать вывод:\n" + out)
        ia, id_, ie, s0, s1, s2 = map(int, m.groups())
        si, ss = ia + id_ + ie, s0 + s1 + s2
        rows.append([name, n, SECS, ia, id_, ie, s0, s1, s2, si, ss,
                     round(si / SECS), round(ss / SECS)])

hdr = ["lock", "len", "sec", "i_asc", "i_desc", "i_eq", "sw0", "sw1", "sw2",
       "sum_i", "sum_swaps", "i/s", "swaps/s"]
with open(os.path.join(here, "results.csv"), "w") as f:
    f.write(",".join(hdr) + "\n")
    for r in rows:
        f.write(",".join(map(str, r)) + "\n")
with open(os.path.join(here, "results.md"), "w") as f:
    f.write("| " + " | ".join(hdr) + " |\n")
    f.write("|" + "---|" * len(hdr) + "\n")
    for r in rows:
        f.write("| " + " | ".join(map(str, r)) + " |\n")
print(open(os.path.join(here, "results.md")).read())