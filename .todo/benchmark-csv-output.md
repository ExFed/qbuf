# Benchmark CSV Output

As a performance analyst, I want the benchmark to emit CSV results so that I
can import them into spreadsheets for trend analysis.

## Goals

- [x] Capture SPSC, MutexQueue, and MmapSPSC metrics in a CSV file alongside
  the text report.
- [x] Allow users to specify the CSV output path or fall back to a default.
- [x] Document how to enable CSV export in the README or supporting docs.

## Non-goals

- Automating CSV ingestion into external dashboards.
- Altering the existing console summary formatting.

## Implementation Steps

- [x] Review current benchmark reporting logic in src/benchmark.cpp.
- [x] Design a CLI flag or config option to enable CSV output and choose the
  file path.
- [x] Implement CSV serialization that covers all recorded benchmark metrics.
- [x] Add a regression test or script that validates CSV generation succeeds.
- [x] Update the README with a usage example and expected CSV output snippet.

## Assumptions

- Existing benchmark runs already gather the metrics needed for CSV export.
- Users execute the benchmark in environments where file creation is allowed.
- CSV output can rely on standard library facilities without new dependencies.

## Risks

- CSV writes might fail silently if the destination path is unwritable.
