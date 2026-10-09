# Conservative LMR results — 9 October 2026

LMR is implemented as an opt-in search feature; the default remains the established full-depth history engine. It reduces by one ply only at non-root nodes with at least four plies remaining, starting with the fifth ordered move. Captures, capture continuations, promotions, TT moves, killers, and history scores of at least 1024 are exempt. A reduced null-window result above alpha triggers a full-depth search with the original window. Capture-chain depth and turn handling, quiescence, and draw rules are unchanged. Re-searches are included in the node counter. No LMP or additional pruning is introduced.

The frozen 100-position corpus, model, compiler options, tablebases, and node-count convention are the same as [the previous suite](suite-results.md). Corpus SHA-256: 773A3C3A13E81A06467CD54F1A9CAA0384F17117DBA69A7281FFC0FC65EC1BAC. Checkpoint: nnue_best_v2.bin, step 22000. The LMR-off baseline reproduces every saved baseline score, node count, and search counter exactly at both depths.

Three runs at each depth were made after compilation finished. Nodes, scores, score differences, and all search counters repeated exactly. Wall-clock figures are medians of total search time across the 100 positions, with observed ranges shown separately. TT clearing, corpus replay, and loading are outside the search timer. Position timing columns in the detailed CSVs are from the first trial; [all aggregate timing trials](lmr-timings.csv) are retained.

| Depth | Full-depth nodes | LMR nodes | Node reduction | Geo. mean full/LMR | Median full ms | Median LMR ms | Changed scores / 100 | Mean absolute score delta | Max absolute delta |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 1,005,313 | 700,733 | 30.30% | 1.2748 | 1857.48 | 1653.28 | 15 | 1.79 | 31 |
| 10 | 5,144,857 | 3,080,859 | 40.12% | 1.4605 | 10079.62 | 8798.97 | 19 | 3.04 | 49 |

| Depth | Full-depth time range ms | LMR time range ms | Reduced searches | Full-depth re-searches | Fewer/equal/more nodes across positions |
|---:|---:|---:|---:|---:|---|
| 8 | 1230.24–3152.74 | 1295.08–1684.08 | 11,443 | 920 | 71/26/3 |
| 10 | 10040.42–11928.14 | 7023.07–9444.78 | 56,792 | 5,304 | 74/23/3 |

Score deltas are in the engine's integer evaluation units, not centipawns. A higher selective score does not establish a better move. Unlike the earlier ordering-only stages, LMR intentionally sacrifices complete full-depth coverage of late moves, so unchanged scores are not guaranteed. Node savings are repeatable; timing and playing-strength claims have additional uncertainty.

| Depth | Phase (20 positions) | Full nodes | LMR nodes | Reduction | Geo. mean | Median full ms | Median LMR ms | Changed scores | Max absolute delta |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | opening | 374,920 | 249,381 | 33.48% | 1.4258 | 579.96 | 539.86 | 7 | 28 |
| 8 | middle | 321,960 | 231,466 | 28.11% | 1.3797 | 514.08 | 530.37 | 5 | 23 |
| 8 | captures | 116,861 | 86,316 | 26.14% | 1.2879 | 224.36 | 172.13 | 3 | 31 |
| 8 | kings | 159,308 | 108,123 | 32.13% | 1.3124 | 310.64 | 253.01 | 0 | 0 |
| 8 | endgame | 32,264 | 25,447 | 21.13% | 1.0124 | 75.33 | 61.91 | 0 | 0 |
| 10 | opening | 1,860,866 | 1,101,790 | 40.79% | 1.6209 | 3748.47 | 3277.06 | 6 | 49 |
| 10 | middle | 1,503,086 | 892,597 | 40.62% | 1.6913 | 2370.02 | 2150.80 | 9 | 40 |
| 10 | captures | 672,322 | 390,455 | 41.92% | 1.4729 | 1368.14 | 1019.37 | 3 | 12 |
| 10 | kings | 844,427 | 392,550 | 53.51% | 1.6574 | 2112.58 | 1031.51 | 1 | 46 |
| 10 | endgame | 264,156 | 303,467 | -14.88% | 0.9930 | 676.07 | 755.27 | 0 | 0 |

The captures stratum describes positions with mandatory captures at the root. Savings in those positions come from reducible quiet moves later in the tree; captures themselves are not reduced. Endings include tablebase-assisted searches and repetition histories that require direct search.

Self-play used the same checkpoint on both sides, LMR on for A and off for B, 100 ms per move, and 50 seeded eight-turn random openings, each played twice with colors reversed (100 games). TT, killers, and history were reset between games. Games finished through existing terminal/draw rules; no evaluation adjudication was used. Boards and PV turns were checked for agreement throughout.

LMR result: **41 wins, 37 losses, 22 draws; 52.0% score**. A 10,000-resample percentile bootstrap over the 50 complete opening pairs (seed 42) gives an approximate 95% score interval of **46.5%–58.0%**. Pair resampling preserves the dependence between the two games from each opening.

The interval includes 50%, so this pilot does not establish a playing-strength improvement. LMR remains off by default, available through the constructor flag and comparison tools.

An earlier execution stopped after 14 games without a final result; that partial run is excluded. The completed 100-game run exited with code 0 and no stderr errors.

Raw games: [match log](lmr-selfplay.txt), [per-game results](lmr-selfplay.csv). The existing runner prints an observed Elo point estimate and mean completed depths; those are descriptive, not proof of strength. Tablebase-assisted positions can inflate the completed-depth averages.

Validation: native main, trainNNUE, bench, suiteBench, headlessMatch, and orderingTests rebuilt. CTest passed, including LMR guard checks, positive reduction and re-search exercises, legal complete PVs, capture-chain preservation, timed-search restoration, and the unchanged corpus validation. No WebAssembly rebuild, broader checkpoints/time controls, or larger strength trial was performed.

Per-position metrics and score changes: [depth 8 CSV](lmr-depth8.csv), [depth 10 CSV](lmr-depth10.csv). Reproduce from build/bin:

```powershell
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/lmr-depth8.csv --lmr
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/lmr-depth10.csv --lmr
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 100 --lmr
ctest --test-dir .. --output-on-failure
```
