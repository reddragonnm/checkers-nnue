# Move-ordering benchmark — 9 October 2026

Initial checkers position, iterative deepening, fresh TT/killers/history for each target depth. NNUE checkpoint: nnue_best_v2.bin, step 22000. Native Release build with GCC 14.1.0, C++23, -O3 -march=native -ffast-math. Existing WDL and DTZ tables loaded; no tablebase hits in these searches.

Model SHA-256: CAFF58D8D888972F8932F4FDC62FF9F8948D8AC993F6A633C6C2330610614C41.

The original engine was rebuilt and reproduced the supplied node counts exactly at every depth from 1 to 15. TT-first ordering was already implemented, so it is the baseline. The revised TT stage also reproduces those counts. Tactical adds captured-king and promotion scores, killers adds two quiet killers per ply, and history adds bounded side/from/to quiet history. The shared engine defaults to history.

Three repeated runs of bench.exe 15 all were made after compilation finished. Nodes, scores, TT counters, and cutoff counters matched across all three runs; all stages returned identical scores at every depth. Timings below are medians with observed ranges. Large timing variation remains, so these runs establish node savings rather than a reliable elapsed-time speedup. The earlier run made alongside compilation is excluded.

| Depth-15 stage | Nodes | Fewer nodes vs TT | Median ms | Observed ms range | Median NPS | Beta cutoffs | First-move cutoff % | TT hit % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| tt | 56,92,729 | 0.00% | 4508.20 | 4412.22–21343.87 | 12,62,748 | 11,92,527 | 82.86% | 14.39% |
| tactical | 56,92,197 | 0.01% | 7133.64 | 4364.52–11161.07 | 7,97,937 | 11,92,778 | 82.87% | 14.39% |
| killers | 50,46,742 | 11.35% | 10156.62 | 3939.89–10651.64 | 4,96,892 | 10,43,645 | 90.96% | 16.40% |
| history | 51,28,270 | 9.92% | 8447.40 | 4013.24–16134.08 | 6,07,082 | 10,43,292 | 92.20% | 17.28% |

History is not a win at every depth: at depth 15 it searches 1.62% more nodes than killers alone. At depth 14 it searches 12.55% fewer nodes than killers and 26.67% fewer than TT. At depth 10 it searches 59.34% fewer nodes than TT. Tactical scoring saves just 532 nodes at depth 15 (0.0093%): the starting-position test contains few promotions or kings, so it is a weak measure of that component.

| Depth | TT nodes | + Tactical | + Killers | + History | History reduction vs TT |
|---:|---:|---:|---:|---:|---:|
| 1 | 7 | 7 | 7 | 7 | 0.00% |
| 2 | 54 | 54 | 56 | 56 | -3.70% |
| 3 | 255 | 255 | 265 | 265 | -3.92% |
| 4 | 560 | 560 | 528 | 515 | 8.04% |
| 5 | 1,637 | 1,637 | 1,457 | 1,424 | 13.01% |
| 6 | 3,090 | 3,090 | 2,864 | 2,771 | 10.32% |
| 7 | 11,057 | 11,057 | 9,670 | 9,411 | 14.89% |
| 8 | 24,963 | 24,963 | 21,239 | 19,841 | 20.52% |
| 9 | 88,162 | 88,162 | 88,704 | 68,865 | 21.89% |
| 10 | 3,19,771 | 3,19,633 | 2,01,644 | 1,30,003 | 59.34% |
| 11 | 4,87,011 | 4,86,824 | 3,76,713 | 2,77,840 | 42.95% |
| 12 | 9,03,961 | 9,03,659 | 7,19,147 | 5,45,698 | 39.63% |
| 13 | 13,09,285 | 13,08,838 | 11,91,167 | 10,67,754 | 18.45% |
| 14 | 27,77,042 | 27,76,554 | 23,28,647 | 20,36,358 | 26.67% |
| 15 | 56,92,729 | 56,92,197 | 50,46,742 | 51,28,270 | 9.92% |

Nodes retain the original convention: searched turn switches, including quiescence. First-move cutoff percentage counts only main-search move-loop beta cutoffs, excluding quiescence and TT returns. TT Probes now counts all lookups; TT Matches is the old probe count, TT Hits is depth-sufficient matches, and TT hit rate is Matches / Probes. [Full counters and timings](move-ordering.csv) are provided for all 60 stage/depth combinations.

Validation: orderingTests passes with assertions enabled in Release, covering TT priority, killer rotation, capture exclusion from quiet learning, history bounds/side separation, promotion detection for both colors, king-victim ranking, exact search scores, state restoration, timed search, and a complete multi-jump PV. Native bench, main, trainNNUE, and headlessMatch targets rebuilt successfully.

No LMP or LMR added. Playing strength, tactical-position performance, depths above 15, and WebAssembly compilation were not measured. The versioned matchmaking engines and published docs WebAssembly assets remain unchanged.

Reproduce from build/bin:

```powershell
./bench.exe 15 all
ctest --test-dir .. --output-on-failure
```
