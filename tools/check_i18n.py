#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""KBMacro UI translation self-check.

Columns required by the task (all must be 0 for every target file):
    missing  = keys present in en-US.json but absent from the target file
    extra    = keys present in the target file but absent from en-US.json
    mismatch = keys whose order, placeholders, newline count or
               protected token (KBMacro/DPI/Ctrl/Shift/Alt/Win/×/.json/...)
               differs from en-US.json

Also reports format problems: BOM, CRLF, indent, \\uXXXX escapes,
missing trailing newline, empty / non-string values.

Usage:  python check_i18n.py [file.json ...]
        (no arguments -> checks the 7 required target files)
Exit code 0 only when every column is 0 and no format problem exists.
"""
from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EN_PATH = os.path.join(HERE, "en-US.json")
TARGETS = ["zh-TW.json", "fr-FR.json", "de-DE.json", "ja-JP.json",
           "it-IT.json", "es-ES.json", "ru-RU.json"]

PLACEHOLDER_RE = re.compile(r"%(?:\d+\$)?[0-9.+#\-]*[dsuxXfgeE]")
PROTECTED = [
    (re.compile(r"(?<![A-Za-z0-9_])KBMacro(?![A-Za-z0-9_])"), "KBMacro"),
    (re.compile(r"(?<![A-Za-z0-9_])DPI(?![A-Za-z0-9_])"), "DPI"),
    (re.compile(r"(?<![A-Za-z0-9_])Ctrl(?![A-Za-z0-9_])"), "Ctrl"),
    (re.compile(r"(?<![A-Za-z0-9_])Shift(?![A-Za-z0-9_])"), "Shift"),
    (re.compile(r"(?<![A-Za-z0-9_])Alt(?![A-Za-z0-9_])"), "Alt"),
    (re.compile(r"(?<![A-Za-z0-9_])Win(?![A-Za-z0-9_])"), "Win"),
    (re.compile(r"×"), "×"),
    (re.compile(r"\.json"), ".json"),
    (re.compile(r"\.txt"), ".txt"),
    (re.compile(r"\.exe"), ".exe"),
]


def load_ordered(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    text = raw.decode("utf-8-sig")
    return json.loads(text, object_pairs_hook=lambda pairs: pairs)


def as_dict(pairs):
    return dict(pairs)


def main(argv):
    en_pairs = load_ordered(EN_PATH)
    en = as_dict(en_pairs)
    en_keys = [k for k, _ in en_pairs]

    targets = argv[1:] or TARGETS
    total_missing = total_extra = total_mismatch = 0
    worst = 0

    for name in targets:
        path = name if os.path.isabs(name) else os.path.join(HERE, name)
        problems = []
        if not os.path.exists(path):
            print("== %s == FILE NOT FOUND" % name)
            total_missing += len(en_keys)
            worst = 1
            continue

        with open(path, "rb") as fh:
            raw = fh.read()

        # ---- format checks -------------------------------------------------
        if raw.startswith(b"\xef\xbb\xbf"):
            problems.append("BOM present")
        if b"\r" in raw:
            problems.append("CRLF line endings")
        if b"\\u" in raw:
            problems.append("contains \\uXXXX escape")
        if raw and not raw.endswith(b"\n"):
            problems.append("no trailing newline")
        body = raw.decode("utf-8-sig").rstrip("\n")
        for i, line in enumerate(body.split("\n")[1:-1], start=2):
            m = re.match(r"^( +)", line)
            if m and len(m.group(1)) != 2:
                problems.append("line %d: indent %d != 2" % (i, len(m.group(1))))
                break

        try:
            tgt_pairs = load_ordered(path)
        except Exception as exc:  # noqa: BLE001 - report and continue
            print("== %s == PARSE ERROR: %s" % (name, exc))
            total_missing += len(en_keys)
            worst = 1
            continue

        tgt = as_dict(tgt_pairs)
        tgt_keys = [k for k, _ in tgt_pairs]
        if len(tgt_keys) != len(set(tgt_keys)):
            problems.append("duplicate keys")

        missing = [k for k in en_keys if k not in tgt]
        extra = [k for k in tgt_keys if k not in en_keys]

        mismatch = 0
        details = []

        # order: target order must match en order restricted to shared keys
        common_en_order = [k for k in en_keys if k in tgt]
        common_tgt_order = [k for k in tgt_keys if k in en]
        if common_en_order != common_tgt_order:
            mismatch += 1
            bad = [k for a, b in zip(common_en_order, common_tgt_order) if a != b]
            details.append("order differs (first: %s)" % (bad[0] if bad else "?"))

        for k in en_keys:
            if k not in tgt:
                continue
            src = en[k]
            dst = tgt[k]
            if not isinstance(dst, str) or not dst.strip():
                mismatch += 1
                details.append("%s: empty/non-string value" % k)
                continue
            ph_src = PLACEHOLDER_RE.findall(src)
            ph_dst = PLACEHOLDER_RE.findall(dst)
            if ph_src != ph_dst:
                mismatch += 1
                details.append("%s: placeholders %s != %s" % (k, ph_dst, ph_src))
                continue
            if src.count("\n") != dst.count("\n"):
                mismatch += 1
                details.append("%s: newlines %d != %d"
                               % (k, dst.count("\n"), src.count("\n")))
                continue
            bad_token = None
            for rx, tok in PROTECTED:
                if len(rx.findall(src)) != len(rx.findall(dst)):
                    bad_token = tok
                    break
            if bad_token:
                mismatch += 1
                details.append("%s: protected token %r altered" % (k, bad_token))

        print("== %s == entries=%d missing=%d extra=%d mismatch=%d"
              % (name, len(tgt_keys), len(missing), len(extra), mismatch))
        for d in details[:20]:
            print("   - %s" % d)
        if missing:
            print("   missing keys: %s" % ", ".join(missing[:20]))
        if extra:
            print("   extra keys: %s" % ", ".join(extra[:20]))
        for p in problems:
            print("   ! format: %s" % p)

        total_missing += len(missing)
        total_extra += len(extra)
        total_mismatch += mismatch
        if missing or extra or mismatch or problems:
            worst = 1

    print("TOTAL missing=%d extra=%d mismatch=%d" % (total_missing, total_extra,
                                                     total_mismatch))
    return 1 if worst else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
