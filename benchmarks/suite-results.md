# Fixed-suite move-ordering results — 9 October 2026

100 frozen, unique board positions, with 20 each from openings, middlegames, mandatory captures, quiet king positions, and endings of at most five pieces. The corpus is [positions.txt](positions.txt), generated once with seed 42 from depth-4 TT self-play, random first eight turns, and 20% exploration thereafter. Positions are legal move sequences from the initial board, preserving repetition and draw counters. Categories are exclusive: endings first, then captures, then quiet kings, then quiet openings with at least 20 pieces, otherwise middlegames.

Corpus SHA-256: 773A3C3A13E81A06467CD54F1A9CAA0384F17117DBA69A7281FFC0FC65EC1BAC.

Native Release, GCC 14.1.0, C++23, -O3 -march=native -ffast-math. Model: nnue_best_v2.bin, step 22000, SHA-256 CAFF58D8D888972F8932F4FDC62FF9F8948D8AC993F6A633C6C2330610614C41. Existing WDL and DTZ tables are enabled. All runs start each position with cleared TT, killers, and history, then use iterative deepening to target depth 8 or 10. Node counts retain the original convention of searched turn switches, including quiescence. No time limits or new pruning are used.

Stages are cumulative: TT; TT + tactical capture/promotion scoring; + killers; + history. The history/killers comparison isolates history. Geometric mean is exp(mean(log(reference nodes / new nodes))) across positions: greater than 1 favors the new stack. Total nodes and these paired ratios are primary; first-move cutoff rate is diagnostic. Timings are retained in CSV but are not used to decide the winner.

| Depth | Stage | Total nodes | Reduction vs TT | Geometric mean TT/new | Fewer/equal/more nodes vs TT |
|---:|---|---:|---:|---:|---|
| 8 | tt | 1,231,522 | 0.00% | 1.0000 | 0/100/0 |
| 8 | tactical | 1,232,297 | -0.06% | 0.9986 | 37/38/25 |
| 8 | killers | 1,069,847 | 13.13% | 1.0738 | 54/24/22 |
| 8 | history | 1,005,313 | 18.37% | 1.1036 | 54/24/22 |
| 10 | tt | 6,556,994 | 0.00% | 1.0000 | 0/100/0 |
| 10 | tactical | 6,585,958 | -0.44% | 0.9903 | 33/31/36 |
| 10 | killers | 5,522,732 | 15.77% | 1.0632 | 50/23/27 |
| 10 | history | 5,144,857 | 21.54% | 1.1097 | 47/23/30 |

History beats killers in both aggregate nodes and geometric mean at both tested depths. The captures/promotions stage alone slightly regresses against TT on this corpus. The results support keeping history in the shared engine for now, without claiming that it wins on every position or at every depth.

| Depth | Positions | History node reduction vs killers | Geometric mean killers/history | Fewer/equal/more nodes vs killers |
|---:|---|---:|---:|---|
| 8 | All 100 | 6.03% | 1.0277 | 46/25/29 |
| 8 | 80 roots with >5 pieces | 5.72% | 1.0327 | 45/6/29 |
| 10 | All 100 | 6.84% | 1.0438 | 50/23/27 |
| 10 | 80 roots with >5 pieces | 4.66% | 1.0490 | 48/5/27 |

Endings include tablebase-assisted searches and repetition contexts where the engine searches directly. They are separated below; the 80 larger-material roots also favor history in geometric mean at both depths. Descendant nodes may still reach tablebases from larger-material roots.

| Depth | Phase (20 positions each) | TT nodes | Killers nodes | History nodes | History reduction vs killers | Geometric mean killers/history |
|---:|---|---:|---:|---:|---:|---:|
| 8 | opening | 472,961 | 400,124 | 374,920 | 6.30% | 1.0400 |
| 8 | middle | 407,112 | 348,640 | 321,960 | 7.65% | 1.0421 |
| 8 | captures | 138,212 | 122,225 | 116,861 | 4.39% | 1.0139 |
| 8 | kings | 165,682 | 161,104 | 159,308 | 1.11% | 1.0349 |
| 8 | endgame | 47,555 | 37,754 | 32,264 | 14.54% | 1.0081 |
| 10 | opening | 2,365,307 | 2,007,557 | 1,860,866 | 7.31% | 1.0429 |
| 10 | middle | 1,762,374 | 1,515,912 | 1,503,086 | 0.85% | 1.0187 |
| 10 | captures | 793,876 | 648,759 | 672,322 | -3.63% | 0.9945 |
| 10 | kings | 988,898 | 947,188 | 844,427 | 10.85% | 1.1460 |
| 10 | endgame | 646,539 | 403,316 | 264,156 | 34.50% | 1.0230 |

History regresses on aggregate nodes in the mandatory-capture stratum at depth 10 by 3.63%, even though it wins overall. This is further reason to use a suite and paired node ratios rather than first-move cutoff percentage alone. The single starting-position depth-15 history regression remains valid; it does not conflict with this broader result.

Every position returned the same score across all four stacks at each target depth, and every search restored the board. Both target depths were repeated; nodes, scores, and search counters reproduced exactly. CTest passed, including the full corpus replay/uniqueness/stratum check, malformed input rejection, and geometric-mean self-checks.

Per-position results: [depth 8 CSV](suite-depth8.csv), [depth 10 CSV](suite-depth10.csv). Each includes nodes, scores, root material/capture/draw context, TT/EGTB/beta counters, timing, and the paired ratio to TT. To reproduce from build/bin:

```powershell
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/suite-depth8.csv
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/suite-depth10.csv
ctest --test-dir .. --output-on-failure
```

This is a first stratified corpus from one checkpoint and shallow self-play. Positions from the same game are correlated, and the equal phase quotas are not a measured distribution of tournament play. It is not an Elo/strength test or evidence of reliable wall-clock gains. No LMR or LMP was added; broader game distributions and deeper suite runs remain unmeasured.
