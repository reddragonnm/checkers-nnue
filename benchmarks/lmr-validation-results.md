# LMR validation: timing, uniqueness, and expanded self-play

Made by 6.1 Sol.

The search policy and default are unchanged: guarded LMR remains opt-in, and no LMP is added. The frozen 100-position corpus and checkpoint step 22000 match the previous experiments. All scores, nodes, TT/EGTB/cutoff counters, and LMR diagnostics in 30 fresh unpinned runs and 36 controlled runs match the saved results exactly. No compilation or match ran during suite measurements.

The old depth-8 slowdown did not reproduce consistently in fresh processes. In six pinned trials, guarded LMR's median total search time was 31.25% lower at depth 8 and 52.90% lower at depth 10 than baseline; node savings remained 29.62% and 40.88%, respectively. These timing gains are conditional on this host and workload. The suite has no duplicate positions. The expanded 600-game test did not establish a playing-strength gain: guarded LMR scored 50.75% against baseline and 50.75% against original LMR, with both paired intervals including 50%. The earlier 61.5% pilot advantage did not replicate on the held-out starts.

## Position uniqueness

An independent replay audit compares exact dark-piece, light-piece, and king bitboards plus side to move, without relying on Zobrist hashes. The suite contains 100 unique positions and zero duplicate entries; position 72 occurs once. Its repeated-history flag refers to recurrence along its own game path. Deduplicating the suite changes neither total nodes nor the geometric mean, which already gives each unique position one contribution. The difficult position remains in the corpus. [Exact-board audit](position-uniqueness.csv).

## Timing diagnosis

The old representative slow depth-8 trial affected 97/100 positions. Its two largest increases (positions 3 and 37) account for only 15.9% of the net 694.95 ms slowdown; the effect was spread across the workload. [Old per-position times and nodes](lmr-old-slowdown-positions.csv). Original and guarded LMR have identical nodes and scores on 99 positions at each depth; only position 72 changes work.

Each new run uses one policy in a fresh process. Five unpinned trials rotate policy order. Six further trials pin each process to logical CPU 2 (affinity mask 4), with balanced rotating order, on a host exposing 16 logical processors. Pinning prevents core migration but does not reserve the CPU. Search timers exclude model/tablebase loading, corpus replay, and TT clearing.

| Environment | Depth | Policy | Nodes | Median wall ms | Wall range ms | NPS at median wall time | Median credited CPU ms |
|---|---:|---|---:|---:|---:|---:|---:|
| fresh | 8 | history | 1,005,313 | 1802.64 | 1389.33-2970.32 | 557,691 | not measured |
| fresh | 8 | lmr | 700,733 | 1127.54 | 872.53-6804.90 | 621,471 | not measured |
| fresh | 8 | lmr_endgame | 707,550 | 1595.61 | 739.11-3566.22 | 443,436 | not measured |
| fresh | 10 | history | 5,144,857 | 9573.71 | 7260.71-14449.88 | 537,394 | not measured |
| fresh | 10 | lmr | 3,080,859 | 7731.43 | 5500.96-8470.34 | 398,485 | not measured |
| fresh | 10 | lmr_endgame | 3,041,548 | 8583.39 | 3590.62-12480.90 | 354,353 | not measured |
| pinned | 8 | history | 1,005,313 | 1548.47 | 1245.19-1906.36 | 649,230 | 1164.06 |
| pinned | 8 | lmr | 700,733 | 1162.16 | 846.16-1384.98 | 602,960 | 812.50 |
| pinned | 8 | lmr_endgame | 707,550 | 1064.64 | 916.51-1192.19 | 664,589 | 765.62 |
| pinned | 10 | history | 5,144,857 | 10423.79 | 7186.92-11828.26 | 493,569 | 7359.38 |
| pinned | 10 | lmr | 3,080,859 | 5517.52 | 4465.60-7252.96 | 558,377 | 4226.56 |
| pinned | 10 | lmr_endgame | 3,041,548 | 4909.68 | 4103.16-6921.92 | 619,500 | 3664.06 |

NPS uses the established engine node counter, including re-search visits; it is not a count of every recursive capture-continuation call. CPU time is Windows main-thread user plus kernel time from GetThreadTimes. Recorded CPU increments are coarse (15.625 ms in these runs), so tiny searches can show zero CPU time. Aggregates help distinguish credited computation from elapsed time but cannot precisely assign every millisecond of waiting or identify its cause.

The former 2.07x slowdown is not a repeatable guarded-policy result in the fresh-process experiments. Unpinned timing ranges are wide, and controlled runs retain some variation. Observed wall time exceeding credited CPU time supports runtime scheduling/waiting delays as a contributor; the data do not isolate an exact OS or hardware cause. No search optimization was added to produce the new times.

[All per-position trials](lmr-validation-raw.csv), [all aggregate/category timing trials](lmr-validation-timings.csv), and [side-by-side per-position nodes, median times, NPS, and score changes](lmr-validation-positions.csv) retain the evidence. Timing medians in the main table are sums over the suite per trial; per-position medians in the comparison CSV need not sum to that figure.

## Expanded match protocol

The primary candidate remains the five-piece endgame guard. The comparison is fixed at 200 games per pairing: original LMR vs baseline, guarded LMR vs baseline, and guarded LMR vs original LMR, for 600 games. Every pairing uses the same 100 unique, frozen starting positions, once with each color. Starting positions have no exact-board overlap with the tuning corpus. Seed 20261009 generates 20 positions each after 8, 16, 24, 32, and 40 uniformly random legal complete turns; terminal/drawn positions, duplicates, tuning roots, and roots with five or fewer pieces are rejected. This gives 15 opening, 33 middle, 26 capture, and 26 king positions. These are diverse legal starts, not a tournament opening book.

Both sides use a frozen copy of nnue_best_v2.bin, step 22000, with a nominal 100 ms per move. All match processes use the same logical CPU 2. TT, killers, and history reset between games. Existing terminal/draw rules finish games without evaluation adjudication. Move indices, complete turns, board agreement, and search-hash restoration are checked. Every search records actual elapsed time, nodes, completed depth, material, and move category. No benchmark, compilation, or other match runs concurrently. The 600-game stopping count is fixed before results are inspected; the smoke-test games and prior pilots are excluded.

[Frozen starting paths](strength-openings.txt), [opening metadata](strength-openings.csv). Opening SHA-256: E412A5002E9A52525B72E907E04E6D0651B9E4969E87E8ADFA468D87AC2D145D. Checkpoint SHA-256: CAFF58D8D888972F8932F4FDC62FF9F8948D8AC993F6A633C6C2330610614C41. Corpus SHA-256: 773A3C3A13E81A06467CD54F1A9CAA0384F17117DBA69A7281FFC0FC65EC1BAC.


## Expanded match results

All 600 games finished. Each pairing has 100 complete opening pairs. Approximate 95% score intervals use 10,000 percentile bootstrap resamples of the complete opening pairs (seed 20261009). Pair resampling keeps the two color-swapped games together. Intervals are calculated separately for each pairing; they are not simultaneous multi-comparison intervals. Guard vs baseline is the primary candidate comparison. Prior pilots and integration smoke games are not pooled with these results.

| A policy vs B policy | A wins | A losses | Draws | A score | Approx. paired 95% interval |
|---|---:|---:|---:|---:|---|
| original-baseline | 89 | 79 | 32 | 52.50% | 50.00-55.00% |
| guard-baseline | 89 | 86 | 25 | 50.75% | 48.50-53.00% |
| guard-original | 88 | 85 | 27 | 50.75% | 48.25-53.25% |

Neither guarded comparison establishes an advantage over its opponent. Original LMR's interval touches 50%, so its small positive score is also inconclusive. This does not establish that any policy is weaker or exactly equal in strength. Keep guarded LMR opt-in and retain the frozen fixtures for further replication across checkpoints and time controls. Category scores below are descriptive, with small samples and no category-specific confidence claims.

| Pairing | Opening category | Games | A score |
|---|---|---:|---:|
| original-baseline | opening | 30 | 58.33% |
| original-baseline | middle | 66 | 55.30% |
| original-baseline | captures | 52 | 49.04% |
| original-baseline | kings | 52 | 49.04% |
| guard-baseline | opening | 30 | 53.33% |
| guard-baseline | middle | 66 | 50.76% |
| guard-baseline | captures | 52 | 50.00% |
| guard-baseline | kings | 52 | 50.00% |
| guard-original | opening | 30 | 46.67% |
| guard-original | middle | 66 | 52.27% |
| guard-original | captures | 52 | 51.92% |
| guard-original | kings | 52 | 50.00% |

## Actual elapsed search time

Every search has a nominal 100 ms deadline. Times cover the whole AIPlayer::search call, including iterative-deepening setup and PV completion; move application and CSV writing are outside the timer. Solved/tablebase-assisted roots may return well before the budget and inflate completed-depth averages. Policies reach different game paths, so these elapsed-time distributions are descriptive and are not a fixed-work speed comparison. The fixed suite is the performance comparison.

| Pairing | Policy | Searches | Mean ms | Median ms | p95 ms | Max ms | Above 105 ms | Zero completed depth |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| original-baseline | lmr | 8871 | 73.751 | 100.010 | 101.511 | 203.882 | 2.39% | 0 |
| original-baseline | history | 8863 | 72.647 | 100.010 | 101.940 | 208.865 | 2.91% | 0 |
| guard-baseline | lmr_endgame | 8817 | 72.839 | 100.010 | 100.431 | 1136.522 | 2.54% | 0 |
| guard-baseline | history | 8817 | 71.186 | 100.010 | 100.338 | 163.853 | 2.21% | 0 |
| guard-original | lmr_endgame | 9001 | 72.377 | 100.010 | 100.031 | 188.630 | 1.57% | 0 |
| guard-original | lmr | 8997 | 72.200 | 100.010 | 100.027 | 166.434 | 1.50% | 0 |

All searches completed at least one depth. Median elapsed search time was about 100.01 ms for every policy, and 1.50-2.91% of calls exceeded 105 ms. The largest overrun was guarded LMR at game 67, opening 34: 1,136.522 ms, 1,053 counted nodes, completed depth 8, ten pieces and one king. That low node count is consistent with a pause, but match calls did not record CPU time, so the cause remains unconfirmed. The nominal budget is not a hard guarantee on actual elapsed time; the strength results apply to this observed timing behavior.

Raw games: [original vs baseline](validation-original-baseline.txt), [guard vs baseline](validation-guard-baseline.txt), [guard vs original](validation-guard-original.txt). [All per-game outcomes](validation-games.csv), [scores and intervals](validation-scores.csv), [category scores](validation-category-scores.csv), [elapsed-time summaries](validation-move-timing-summary.csv), [whole-run elapsed time](validation-match-runs.csv). Per-search measurements: [original vs baseline](validation-original-baseline-moves.csv), [guard vs baseline](validation-guard-baseline-moves.csv), [guard vs original](validation-guard-original-moves.csv).

Validation: suiteBench and headlessMatch rebuilt. All three CTest tests passed, including the new legal/unique opening-file check. Duplicate, illegal, out-of-range, empty opening files, mismatched game counts, and attempts to overwrite opening inputs were rejected. A separate two-game integration check verified the direct pairing and timing CSV. All expanded match processes exited 0 with empty stderr, with PV legality, complete-turn and board consistency checks, and search-hash restoration checks throughout. Checkpoint, opening, and corpus hashes remain unchanged. git diff --check passed.

Guarded LMR remains opt-in. LMP, additional checkpoints/time controls, and WebAssembly validation were not tested. The exact OS/hardware cause of elapsed-time variation remains unresolved. Timing gains are conditional on this host/workload; strength results are conditional on this legal-start distribution, checkpoint, and nominal time control.
