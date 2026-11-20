"""Independent menu/process smoke tests; Python is used only by the tester."""
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
BUILD.mkdir(exist_ok=True)
RUN = Path(tempfile.mkdtemp(prefix="e2e-", dir=BUILD)).resolve()
assert RUN.is_relative_to(BUILD.resolve())
checks = []


def check(name, condition):
    if not condition:
        raise AssertionError(name)
    checks.append(name)


def run(executable, lines, evidence):
    start = time.perf_counter()
    result = subprocess.run(
        [str(BUILD / executable)], input="\n".join(lines) + "\n",
        cwd=RUN, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30,
    )
    elapsed = time.perf_counter() - start
    (RUN / (evidence + ".stdout.txt")).write_text(result.stdout, encoding="utf-8")
    (RUN / (evidence + ".stderr.txt")).write_text(result.stderr, encoding="utf-8")
    check(evidence + " successful process exit", result.returncode == 0)
    return result.stdout, elapsed


def rows(path):
    return list(csv.DictReader(io.StringIO(path.read_text(encoding="utf-8"))))


corpus = RUN / "corpus"
corpus.mkdir()
(corpus / "a, first.txt").write_text("Apple apple, banana!\n", encoding="utf-8")
(corpus / "b.txt").write_text("apple orange", encoding="utf-8")
(corpus / "empty.txt").write_text("", encoding="utf-8")
(RUN / "collection.txt").write_text(
    "corpus/a, first.txt\ncorpus/b.txt\ncorpus/empty.txt\n", encoding="utf-8"
)

text, _ = run("coursework.exe", [
    "not-a-number", "4", "1", "corpus", "3", "4", "!!!", "APPLE apple", "",
    "/exit", "6", "report.txt", "7", "report.csv", "8", "index.idx", "0",
], "folder-and-save")
check("invalid menu input handled", "Invalid menu choice" in text)
check("unready search handled", "load a saved index first" in text)
check("punctuation-only query handled", "at least one English letter" in text)
check("folder loaded including empty document", "Loaded 3 documents" in text)
check("ranked results correct", "1\t0\ta, first.txt\t2" in text and "2\t1\tb.txt\t1" in text)
report = rows(RUN / "report.csv")
collection = next(r for r in report if r["section"] == "collection")
check("collection totals", (collection["document_count"], collection["total_words"],
                            collection["unique_words"]) == ("3", "5", "3"))
index_row = next(r for r in report if r["section"] == "index")
check("posting relationships", index_row["posting_relationships"] == "4")
doc_row = next(r for r in report if r["section"] == "document" and r["document_id"] == "0")
check("CSV comma filename parsed", doc_row["filename"] == "a, first.txt")
check("per-document totals", (doc_row["total_words"], doc_row["unique_words"]) == ("3", "2"))

# Only move the test-created corpus, within the verified build directory.
removed = RUN / "corpus-removed"
assert corpus.resolve().is_relative_to(RUN) and removed.resolve().is_relative_to(RUN)
corpus.rename(removed)
text, _ = run("coursework.exe", [
    "9", "index.idx", "3", "4", "apple", "", "/exit", "5", "0",
    "6", "report-after-reload.txt", "7", "report-after-reload.csv", "0",
], "new-process-reload")
check("source corpus unavailable", not corpus.exists())
check("loaded without rebuilding", "Index loaded without rebuilding" in text and "Load + build CPU time" not in text)
check("search after process restart", "1\t0\ta, first.txt\t2" in text)
check("document preview restored", "Apple apple, banana!" in text)
after = rows(RUN / "report-after-reload.csv")
check("statistics preserved after process restart",
      [r for r in report if r["section"] in ("collection", "document", "index")] ==
      [r for r in after if r["section"] in ("collection", "document", "index")])

assert removed.resolve().is_relative_to(RUN) and corpus.resolve().is_relative_to(RUN)
removed.rename(corpus)
text, _ = run("coursework_standard.exe", [
    "1", "collection.txt", "3", "7", "standard-report.csv", "0",
], "standard-c-list")
check("standard C collection-list mode", "Loaded 3 documents" in text)
standard = rows(RUN / "standard-report.csv")
check("standard mode statistics match",
      [r for r in report if r["section"] in ("collection", "document", "index")] == standard)
check("no invented search report before a query", not any(r["section"].startswith("search") for r in standard))

saved = (RUN / "index.idx").read_bytes()
(RUN / "broken.idx").write_bytes(saved[:len(saved)//2])
text, _ = run("coursework.exe", [
    "9", "index.idx", "9", "broken.idx", "4", "apple", "", "/exit", "0",
], "failed-load-retains-index")
check("failed load reported", "Index not loaded" in text)
check("old index usable after failed load", "1\t0\ta, first.txt\t2" in text)

large = RUN / "large"
large.mkdir()
for i in range(50):
    (large / f"doc{i:03}.txt").write_text("apple banana cherry delta " * 500, encoding="ascii")
text, build_seconds = run("coursework.exe", [
    "1", "large", "3", "7", "large-report.csv", "8", "large.idx", "0",
], "synthetic-100000-words")
large_report = rows(RUN / "large-report.csv")
large_collection = next(r for r in large_report if r["section"] == "collection")
check("50 documents and 100000 words", (large_collection["document_count"],
      large_collection["total_words"], large_collection["unique_words"]) == ("50", "100000", "4"))
check("200 large-corpus postings", next(r for r in large_report if r["section"] == "index")["posting_relationships"] == "200")
text, reload_seconds = run("coursework.exe", [
    "9", "large.idx", "4", "apple", "5", "/exit", "0",
], "synthetic-reload")
check("large reload query result", "1\t0\tdoc000.txt\t500" in text and "5\t4\tdoc004.txt\t500" in text)

summary = {"status": "passed", "checks": checks,
           "synthetic_corpus": {"documents": 50, "total_terms": 100000,
                                "unique_terms": 4, "postings": 200},
           "process_wall_seconds": {"load_build_report_save": build_seconds,
                                    "reload_search": reload_seconds},
           "note": "Synthetic scale smoke test, not the prescribed coursework benchmark.",
           "evidence_directory": str(RUN)}
(RUN / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(summary, ensure_ascii=False, indent=2))
