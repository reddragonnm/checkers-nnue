# Endgame LMR diagnosis and tuning

The frozen 100-position suite, checkpoint step 22000, tablebases, depths 8 and 10, and search accounting match the previous LMR experiment. Each of three policies was run three times per depth. All scores, nodes, and counters repeated exactly; all LMR-off baselines match the saved baseline, and instrumented original LMR matches the saved original search. Timing medians are sums over the suite, with all trials retained. No self-play or compilation ran during suite timing. Corpus SHA-256: 773A3C3A13E81A06467CD54F1A9CAA0384F17117DBA69A7281FFC0FC65EC1BAC.

Policies: `--lmr` is unchanged (fifth move onward); `--lmr-late` changes only the move threshold to seventh; `--lmr-endgame` changes only the material guard, exempting nodes with five or fewer pieces. Reductions remain one ply, with the same root, depth, capture, promotion, TT, killer, and history protections. LMR remains off by default. No LMP.

| Depth | Policy | Nodes | Saving vs full depth | Median full ms | Median candidate ms | Changed scores / 100 |
|---:|---|---:|---:|---:|---:|---:|
| 8 | lmr | 700,733 | 30.30% | 773.64 | 1446.15 | 15 |
| 8 | lmr-late | 826,282 | 17.81% | 722.87 | 1708.77 | 6 |
| 8 | lmr-endgame | 707,550 | 29.62% | 648.75 | 1343.70 | 15 |
| 10 | lmr | 3,080,859 | 40.12% | 6137.18 | 6139.64 | 19 |
| 10 | lmr-late | 3,792,266 | 26.29% | 6391.31 | 7101.02 | 9 |
| 10 | lmr-endgame | 3,041,548 | 40.88% | 6522.63 | 6062.98 | 19 |

| Depth | Policy | Category | Full nodes | Candidate nodes | Saving | Changed scores / 20 |
|---:|---|---|---:|---:|---:|---:|
| 8 | lmr | opening | 374,920 | 249,381 | 33.48% | 7 |
| 8 | lmr | middle | 321,960 | 231,466 | 28.11% | 5 |
| 8 | lmr | captures | 116,861 | 86,316 | 26.14% | 3 |
| 8 | lmr | kings | 159,308 | 108,123 | 32.13% | 0 |
| 8 | lmr | endgame | 32,264 | 25,447 | 21.13% | 0 |
| 8 | lmr-late | opening | 374,920 | 294,637 | 21.41% | 3 |
| 8 | lmr-late | middle | 321,960 | 272,029 | 15.51% | 2 |
| 8 | lmr-late | captures | 116,861 | 100,248 | 14.22% | 0 |
| 8 | lmr-late | kings | 159,308 | 129,014 | 19.02% | 1 |
| 8 | lmr-late | endgame | 32,264 | 30,354 | 5.92% | 0 |
| 8 | lmr-endgame | opening | 374,920 | 249,381 | 33.48% | 7 |
| 8 | lmr-endgame | middle | 321,960 | 231,466 | 28.11% | 5 |
| 8 | lmr-endgame | captures | 116,861 | 86,316 | 26.14% | 3 |
| 8 | lmr-endgame | kings | 159,308 | 108,123 | 32.13% | 0 |
| 8 | lmr-endgame | endgame | 32,264 | 32,264 | 0.00% | 0 |
| 10 | lmr | opening | 1,860,866 | 1,101,790 | 40.79% | 6 |
| 10 | lmr | middle | 1,503,086 | 892,597 | 40.62% | 9 |
| 10 | lmr | captures | 672,322 | 390,455 | 41.92% | 3 |
| 10 | lmr | kings | 844,427 | 392,550 | 53.51% | 1 |
| 10 | lmr | endgame | 264,156 | 303,467 | -14.88% | 0 |
| 10 | lmr-late | opening | 1,860,866 | 1,414,198 | 24.00% | 3 |
| 10 | lmr-late | middle | 1,503,086 | 1,073,549 | 28.58% | 4 |
| 10 | lmr-late | captures | 672,322 | 467,295 | 30.50% | 1 |
| 10 | lmr-late | kings | 844,427 | 536,649 | 36.45% | 1 |
| 10 | lmr-late | endgame | 264,156 | 300,575 | -13.79% | 0 |
| 10 | lmr-endgame | opening | 1,860,866 | 1,101,790 | 40.79% | 6 |
| 10 | lmr-endgame | middle | 1,503,086 | 892,597 | 40.62% | 9 |
| 10 | lmr-endgame | captures | 672,322 | 390,455 | 41.92% | 3 |
| 10 | lmr-endgame | kings | 844,427 | 392,550 | 53.51% | 1 |
| 10 | lmr-endgame | endgame | 264,156 | 264,156 | 0.00% | 0 |

The fresh timing results do not reproduce the earlier across-the-board wall-clock improvement. At depth 8 every selective policy is slower despite fewer counted nodes. At depth 10 original LMR is effectively flat, the later threshold is slower, and the endgame guard saves about 7.05% against its accompanying baseline. Timings are from the instrumented build and vary between trials; node counts alone cannot establish a speed or strength gain.

## Endgame diagnostics

Endgame counters classify the parent node by actual material (five or fewer pieces), independently of the root category. The table below uses only the 20 endgame roots. Reduced results above alpha always receive a full-depth re-search; accepted reduced-only beta cutoffs are zero in every run. Confirmed cutoffs count re-searched moves that cut off their parent, rather than cutoffs inside descendants. Call-work counters include their initial node and descendants, including nested LMR, so they overlap and must not be summed as exclusive parts of total nodes. Aborted calls contribute attempts and work but no accepted cutoff.

| Depth | Policy | Reductions | Re-searches | Re-search rate | Confirmed beta cutoffs | Reduced call work | Re-search call work |
|---:|---|---:|---:|---:|---:|---:|---:|
| 8 | lmr | 396 | 101 | 25.51% | 2 | 7,152 | 8,991 |
| 8 | lmr-late | 260 | 64 | 24.62% | 2 | 4,355 | 9,320 |
| 8 | lmr-endgame | 0 | 0 | 0.00% | 0 | 0 | 0 |
| 10 | lmr | 4,330 | 2,137 | 49.35% | 27 | 159,999 | 204,439 |
| 10 | lmr-late | 2,570 | 1,279 | 49.77% | 14 | 93,108 | 129,611 |
| 10 | lmr-endgame | 0 | 0 | 0.00% | 0 | 0 | 0 |

The entire depth-10 endgame regression comes from position 72. Its five pieces are all kings, the draw counter is 13, and `hasRepeatedPosition()` is true. The tablebase reports WDL 2 (win) and DTZ 26, but the search deliberately bypasses tablebase results for repeated history. Original LMR attempts 4,330 reductions in that fallback search, re-searches 2,137 (49.35%), and confirms only 27 beta cutoffs. The later threshold still re-searches 49.77% of reductions and does not remove the regression. The endgame guard restores all 20 endgame positions to baseline work. This supports a targeted exemption on this suite; it does not establish that all endgames behave like position 72.

On this corpus, the guard changes only position 72's node count at either depth; all 100 scores and the other 99 node counts match original LMR. At depth 8, position 72 needs 31,219 nodes with the guard versus 24,402 with original LMR, so the guard gives up that shallower saving. At depth 10 it saves 39,311 nodes relative to original LMR.

Per-position endgame comparison (depth 10), including unchanged positions:

| Position | Full nodes | Original LMR | Late LMR | Endgame guard | Original reductions / re-searches |
|---:|---:|---:|---:|---:|---|
| 58 | 20 | 20 | 20 | 20 | 0 / 0 |
| 59 | 60 | 60 | 60 | 60 | 0 / 0 |
| 60 | 70 | 70 | 70 | 70 | 0 / 0 |
| 61 | 60 | 60 | 60 | 60 | 0 / 0 |
| 62 | 60 | 60 | 60 | 60 | 0 / 0 |
| 63 | 60 | 60 | 60 | 60 | 0 / 0 |
| 64 | 40 | 40 | 40 | 40 | 0 / 0 |
| 65 | 30 | 30 | 30 | 30 | 0 / 0 |
| 66 | 30 | 30 | 30 | 30 | 0 / 0 |
| 67 | 60 | 60 | 60 | 60 | 0 / 0 |
| 68 | 50 | 50 | 50 | 50 | 0 / 0 |
| 69 | 60 | 60 | 60 | 60 | 0 / 0 |
| 70 | 80 | 80 | 80 | 80 | 0 / 0 |
| 71 | 60 | 60 | 60 | 60 | 0 / 0 |
| 72 | 262,459 | 301,770 | 298,878 | 262,459 | 4330 / 2137 |
| 73 | 40 | 40 | 40 | 40 | 0 / 0 |
| 74 | 120 | 120 | 120 | 120 | 0 / 0 |
| 75 | 10 | 10 | 10 | 10 | 0 / 0 |
| 76 | 40 | 40 | 40 | 40 | 0 / 0 |
| 77 | 747 | 747 | 747 | 747 | 0 / 0 |

Detailed per-position CSVs: [original depth 8](lmr-tuning-lmr-depth8.csv), [original depth 10](lmr-tuning-lmr-depth10.csv), [late depth 8](lmr-tuning-lmr-late-depth8.csv), [late depth 10](lmr-tuning-lmr-late-depth10.csv), [guard depth 8](lmr-tuning-lmr-endgame-depth8.csv), [guard depth 10](lmr-tuning-lmr-endgame-depth10.csv). [All timing trials](lmr-tuning-timings.csv).

## Paired strength pilot

The endgame guard was selected for self-play because it removes the measured endgame regression while retaining the largest depth-10 node saving. The seventh-move policy was not taken into a match: it loses substantial suite savings and still regresses on position 72. Selection was based on this suite, not a held-out corpus.

Player A used `--lmr-endgame`; B used full-depth history search. Both loaded nnue_best_v2.bin, step 22000. The run used 50 seeded eight-turn random openings (seed 42), each played twice with colors reversed, 100 ms per move, and existing terminal/draw rules without evaluation adjudication. TT, killers, and history reset between games; PV legality and board consistency were checked. No suite benchmark or compilation ran concurrently. This is a separate 100-game pilot; it is not pooled with the earlier original-LMR trial.

Endgame-guard result: **46 wins, 23 losses, 31 draws; 61.5% score**. A 10,000-resample percentile bootstrap over complete opening pairs (seed 42) gives an approximate 95% score interval of **55.5%–67.5%**.

The paired interval excludes 50% in favor of the guard, providing evidence of improved strength under this pilot's conditions. This is not a direct guard-versus-original-LMR match, and broader strength confirmation is still needed before changing the default.

A small fixed pilot at one checkpoint and one time control cannot establish a general strength improvement. LMR remains off by default. No automatic stopping or default promotion was tied to interim match results.

Raw results: [match log](lmr-endgame-selfplay.txt), [per-game results](lmr-endgame-selfplay.csv). Engine exit code was 0 with empty stderr.

Validation: native main, trainNNUE, bench, suiteBench, headlessMatch, and orderingTests rebuilt successfully. Both CTest tests passed, including policy thresholds, piece-count boundaries, reductions and re-searches, counter reset/bounds, legal PVs, complete capture chains, board restoration, and corpus validation. Unknown CLI flags and odd paired-game counts were rejected. Frozen corpus hash is unchanged; git diff --check passed.

No LMP, WebAssembly rebuild, additional checkpoints/time controls, held-out suite, or larger strength test was performed.

Reproduce from build/bin (change depth to 8 for the shallower suite):

```sh
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/candidate.csv --lmr-endgame
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/late-candidate.csv --lmr-late
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 100 --lmr-endgame
```
