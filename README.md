# Checkers NNUE

A checkers engine built from scratch combining a self-trained neural network evaluator, alpha-beta search with aspiration windows, and a complete retrograde endgame tablebase — playable natively on desktop or in the browser via WebAssembly.

---

## Table of Contents

- [Overview](#overview)
- [Board Representation](#board-representation)
- [Neural Network Evaluation (NNUE)](#neural-network-evaluation-nnue)
- [Search Algorithm](#search-algorithm)
- [Endgame Tablebase (EGTB)](#endgame-tablebase-egtb)
- [Training System](#training-system)
- [Matchmaking Framework](#matchmaking-framework)
- [Web Version](#web-version)
- [Building and Running](#building-and-running)

---

## Overview

The project is structured as four independently developed subsystems that are composed together into the final engine:

| Subsystem | Role |
|---|---|
| **Board** | Rules, move generation, position hashing |
| **NNUE** | Position evaluation via self-trained neural network |
| **Search** | Game-tree search with pruning and time management |
| **EGTB** | Perfect endgame play for all positions with ≤5 pieces |

The training, tablebase generation, benchmarking, matchmaking, and playing interfaces are all separate executables sharing the same core headers.

---

## Board Representation

The board is represented using bitboards — one 64-bit integer per piece type. Three bitboards are maintained simultaneously: one for dark pieces, one for light pieces, and one for kings (which overlaps with both colour boards). This representation allows move generation to be implemented almost entirely with bitwise operations and is cache-friendly.

Because checkers is played only on dark squares, only 32 of the 64 board squares are used. A fixed mapping converts between the 32 playable square indices and their positions in the 64-bit integers. This mapping alternates based on which row a square is in.

Move generation produces all legal moves as a list of encoded integers. Each move encodes its origin square, destination square, and capture information in a compact format. Captures are mandatory in checkers — if any capture is available, only captures may be played. Multi-jump sequences are handled by tracking whether the current player is mid-capture; when a piece lands after a capture and can capture again, the turn does not switch until the sequence is complete.

An undo stack records enough information to reverse any move, allowing the search to make and unmake moves without copying the board. The hash is updated incrementally on each make and undo using Zobrist hashing — each piece type on each square has a precomputed random value, and the current hash is the XOR of all occupied piece-square values plus flags for whose turn it is and whether the game is mid-capture.

---

## Neural Network Evaluation (NNUE)

NNUE (Efficiently Updatable Neural Network) is a style of neural network architecture designed for game engines where positions change incrementally. Rather than recomputing the network from scratch each time a move is made, only the part of the computation that changed is updated.

### Architecture

The network has four layers with sizes 128 → 256 → 32 → 1. The input layer encodes the presence of each piece type on each playable square as a binary feature — dark pawn, dark king, light pawn, light king — giving 128 total inputs. The two hidden layers use a clamped ReLU activation that clips outputs to the range [0, 1]. The final output is a single scalar representing the evaluation from the perspective of the side to move.

### Dual Accumulator

The key efficiency insight is the accumulator: the expensive first matrix-vector multiplication (128 → 256) is never recomputed from scratch during search. Instead, when a piece is added or removed from the board, only the corresponding column of the weight matrix is added or subtracted from the running accumulator.

Two accumulators are maintained in parallel — one for the normal board orientation and one for the board mirrored top-to-bottom with colours swapped. This means evaluating from either side's perspective costs nothing extra; you simply choose which accumulator to pass through the remaining layers. The mirrored accumulator is updated simultaneously with the normal one on every move.

### Output Scaling

The raw network output is scaled and clamped to a range that keeps it well below the score values reserved for tablebase wins and forced mate sequences. This ensures the search can always distinguish between a tablebase result and a heuristic evaluation.

---

## Search Algorithm

The search uses negamax — a simplified form of alpha-beta — where a single recursive function evaluates positions from the perspective of the current player and returns the best score achievable.

### Iterative Deepening and Aspiration Windows

The search is run iteratively, starting at depth 1 and increasing by 1 each iteration. This means shallower results are always available as a fallback if time runs out, and move ordering from shallow searches dramatically improves pruning at deeper depths.

Aspiration windows narrow the alpha-beta window around the score from the previous iteration. A narrow window causes many more cutoffs, making the search faster. If the result falls outside the window (a fail-low or fail-high), the search is re-run with a wider window. The window is widened asymmetrically — failing low narrows beta, failing high narrows alpha — and grows by a fixed fraction on each retry.

### Transposition Table

A large hash table stores results from previously evaluated positions. Each entry records the position hash, the score found, the best move, the depth searched, and whether the score is an exact value, a lower bound, or an upper bound. On revisiting a position, if the stored depth is sufficient, the stored result can be returned immediately or used to narrow the window. The best move from the table is always tried first, which is the single most important move ordering heuristic.

### Quiescence Search

At depth zero, the search continues to resolve mandatory capture sequences. Stopping mid-capture would give inaccurate evaluations. At a quiet position it returns the static evaluation; when captures exist, it searches them without a stand-pat cutoff, because passing a mandatory capture is illegal.

### Move Ordering

The shared engine ranks moves by TT move, promotions and captured kings, two quiet killers per ply, and quiet-move history indexed by side/from/to. Generator indices stay unchanged so TT entries and PV moves remain valid. Capture ordering also applies in quiescence. Since moves encode individual jumps, capture scores use the immediate victim rather than exploring entire chains ahead of search.

Quiet beta cutoffs update killers and add a depth-squared history bonus with bounded gravity to prevent overflow. History is halved at the start of each search; killers and history survive iterative deepening and successive turns. `resetTT()` clears all three tables for a fresh game or benchmark. Killer storage covers 256 plies; deeper nodes still use TT, tactical, and history ordering.

Conservative LMR is available through the final `AIPlayer` constructor argument, `useLMR`, and remains off by default pending strength evidence. At non-root nodes with at least four plies remaining, the fifth and later ordered moves may receive a one-ply reduction. Captures, capture continuations, promotions, TT moves, killers, and moves with history scores of at least 1024 are exempt. The reduced search uses a null window around alpha; a result above alpha is re-searched at full depth and the original window. Quiescence and multi-jump turn/depth handling are unchanged. Unlike ordering alone, LMR is selective and may change scores. No LMP is added.

### Multi-Capture Handling

Multi-jump captures require special treatment throughout the search. When a move is made and the same player must continue capturing, the depth does not decrease and the ply does not increment — the continuation is treated as part of the same half-move. The principal variation is built to include the entire capture sequence up to the point where the turn finally switches.

### EGTB Integration

When the total number of pieces on the board drops to five or fewer, the search probes the endgame tablebase at non-root nodes unless the reversible history already contains a repeated position. It also searches instead of using a tablebase win or loss when the distance to the next zeroing move reaches the 80-ply draw limit. A tablebase win returns a score that reflects how quickly the win can be forced — shorter paths score higher. A tablebase loss returns a score that reflects how long the loss can be delayed — longer resistance scores higher. A tablebase draw returns zero, and the search continues normally from the root to find the best drawing move rather than returning immediately.

The tablebase guides move selection in endings with five or fewer pieces and in lines that reach them. Repetition history and the draw counter can change the practical result, so the search handles those positions directly.

### Principal Variation

The principal variation — the sequence of best moves — is collected during search using a triangular PV table. Each node in the search tree receives its own PV vector which is populated when a new best move is found. For multi-capture sequences, child PV moves are appended only when the turn has not yet switched, ensuring the returned PV always contains exactly the moves needed to complete a single turn. A post-search step validates the PV and uses the transposition table to fill in any gaps left by TT hits during mid-capture continuations.

---

## Endgame Tablebase (EGTB)

The tablebase covers all legal checkers positions with five or fewer pieces on the board combined. It stores two types of data: WDL (Win/Draw/Loss) for perfect game-theoretic outcome, and DTZ (Distance-To-Zero) for the optimal number of moves to convert a win or delay a loss.

### Indexing

Positions are encoded using a three-level combinadic index. The first level encodes which squares are occupied, the second encodes which of those squares are dark pieces versus light pieces, and the third encodes which pieces are kings. Combining these three independently enumerated levels gives a compact, collision-free index into a flat array. The total table size for a given material configuration is the product of the three ranges.

For light-to-move positions, the board is flipped vertically and the colours are swapped before indexing. This means the tablebase only needs to be built from one side's perspective — the symmetric case is handled by transformation at probe time.

### WDL Build — Retrograde Analysis

The WDL table is built using retrograde analysis, working backwards from terminal positions. Terminal positions — where one side has no pieces or no legal moves — are assigned WIN or LOSS immediately. Then, iteratively:

- A position is a WIN if any of its successors is a LOSS for the opponent.
- A position is a LOSS if all of its successors are WINs for the opponent.
- A position remains unresolved until both conditions can be definitively checked.

The process repeats in passes until no new positions are resolved in a complete pass. All remaining unresolved positions are draws — they can neither be forced into a loss nor can the opponent be forced into a loss.

Multi-jump captures within a position are handled iteratively using an explicit stack to avoid the complexity of nested recursion while correctly tracking board state through intermediate capture steps.

Tables are built in order of increasing total piece count so that child positions (which always have fewer pieces after a capture) are always already resolved when a parent is evaluated.

### DTZ Build

After WDL is complete, the DTZ table is built with a similar iterative process. For WIN positions, DTZ is the minimum number of moves to reach a position from which the opponent is in LOSS. For LOSS positions, DTZ is the maximum number of moves before the opponent can force a position from which the current player is in LOSS. Zeroing moves — captures and promotions that reset the draw counter — always set DTZ to the minimum value for a won position.

The DTZ tables are large (~193 MB) because they store one full byte per position rather than the two-bit WDL encoding. The WDL tables are ~52 MB.

### Persistence

Both tables are saved as binary files after building. On subsequent runs they load in seconds. Building from scratch takes several minutes.

---

## Training System

The NNUE is trained entirely from self-play using experience replay.

### Training Loop

Training begins with a warmup phase where games are played with a simple material count evaluator. These games populate a replay buffer with diverse positions that the neural network has not yet seen.

Once the buffer has enough entries, the main training phase begins. Games are played using the NNUE at a fixed search depth. To encourage exploration and prevent the network from collapsing onto repetitive play, a fraction of moves are chosen randomly. After each game, the positions encountered are added to the replay buffer (which has a fixed capacity; oldest entries are discarded when full). Then a number of training steps are performed by sampling random batches from the buffer and updating the network weights.

### Target Generation

Training targets are the scores produced by the search engine itself. The idea is that the network learns to approximate the deeper search — a position that the depth-6 search evaluates as strongly winning should also be evaluated that way by the network alone. The targets are normalized to a small range to keep gradients stable.

### Optimizer

The network is trained with the Adam optimizer, which adapts the learning rate for each parameter individually based on first and second moment estimates of the gradient. This makes training robust to the varying scales of different weight gradients across layers.

### Checkpoint Evaluation

Periodically, the current network is evaluated against the previous best checkpoint by running a fixed number of games. If the new version wins more than it loses, it is promoted to the new best model. This prevents regressions from noisy training updates.

---

## Matchmaking Framework

Two versioned engine implementations live in a separate matchmaking directory. Each version is a self-contained engine with its own evaluation, search, and tablebase integration. The matchmaking binary runs automated games between them with a graphical display, alternating colours each game, and prints cumulative results.

This framework was used during development to validate improvements. The primary comparison was between a simple piece-count evaluator (v1) and the full NNUE + EGTB engine (v2). It was also the primary testing ground for the PV extraction fixes, the EGTB draw handling, and the multi-capture correctness work.

---

## Web Version

The engine is compiled to WebAssembly using Emscripten. A thin C API exposes all game functionality — initialising the engine, resetting the game, querying piece positions and legal moves, making human moves, running the AI search, and executing moves from the principal variation one step at a time (to allow animating multi-capture sequences with delays).

The JavaScript frontend communicates entirely through this C API via Emscripten's function wrapping. The board is rendered in HTML with square and piece state queried from the engine on every render. The AI search is time-limited rather than depth-limited in the web version to ensure responsiveness.

The web build uses a smaller transposition table than the native build to respect browser memory constraints. The EGTB is not included in the web version — at ~245 MB combined it would make the initial download impractical. The NNUE alone is sufficient for strong play in the browser.

---

## Building and Running

**Requirements (native):** CMake 3.28+, a C++23 compiler. SFML is fetched automatically.

**Requirements (web):** Emscripten.

```bash
# Native
cmake -B build && cmake --build build -- -j$(nproc)

# WebAssembly
emcmake cmake -B build-web && cmake --build build-web -- -j$(nproc)
```

**Binaries:**

| Binary | What it does |
|--------|-------------|
| `main` | Play against the AI with an SFML window |
| `matchmake` | Run automated AI vs AI games |
| `trainNNUE` | Run the self-play training loop |
| `bench` | Benchmark search speed at depths 1–20 |
| `suiteBench` | Compare move-ordering stages on the fixed 100-position corpus |
| `orderingTests` | Check ranking, cutoff learning, score equivalence, and PV/board restoration |

Run cumulative ordering comparisons from `build/bin`:

```powershell
./bench.exe 15 all
./bench.exe 15 history
ctest --test-dir .. --output-on-failure
```

Stages are `tt` (the original ordering), `tactical` (+ captures/promotions), `killers` (+ killers), and `history` (+ history, the default engine behavior). Every depth starts with cleared TT, killers, and history, then performs iterative deepening. `all` verifies equal final scores across stages. The benchmark uses `nnue_best_v2.bin` and existing WDL/DTZ tables in the working directory.

`Beta Cuts` and `First Cuts` count searched-move cutoffs in the main search, excluding quiescence and TT returns. `First %` is `100 * First Cuts / Beta Cuts`. `TT Probes` now counts all table lookups (the old column counted matching hashes); `TT Matches` preserves that old count, `TT Hits` counts depth-sufficient matches, and `TT Hit %` is `100 * TT Matches / TT Probes`. `Nodes` retains the original count of searched turn switches, including quiescence, so it can be compared with older benchmarks.

The initial-position results and timing limitations are recorded in [the ordering benchmark report](benchmarks/move-ordering.md). Opening benchmarks contain few kings or promotions; use tactical positions and matches before drawing conclusions about playing strength.

The fixed suite in `benchmarks/positions.txt` contains 20 unique positions each from openings (at least 20 pieces), middlegames, mandatory captures, quiet positions with kings, and endings with at most five pieces. It is frozen from seeded depth-4 TT self-play with random openings and 20% exploration. Each position records the legal encoded moves from the initial board, preserving repetition and draw-counter context when replayed.

```powershell
# From build/bin
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/suite-depth8.csv
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/suite-depth10.csv
```

Every position gets a fresh TT, killers, and history, followed by iterative deepening to the same fixed target depth. The runner compares the same four cumulative stacks as `bench`, checks score equivalence and board restoration, writes per-position counters to CSV, and reports total nodes plus the geometric mean of `TT nodes / stage nodes`. A ratio above 1 means reduced search work. The comparison between `killers` and `history` isolates history because both retain tactical ordering. First-move cutoffs are diagnostic, not the primary ranking metric.

See [the fixed-suite results](benchmarks/suite-results.md) for overall and per-phase comparisons. The ending stratum is reported separately because tablebase access and repetition history affect these searches. CTest validates all 100 frozen move sequences, unique boards, the five strata, malformed input rejection, and geometric-mean calculations. To deliberately create a replacement corpus at a new path, run `./suiteBench.exe generate new-positions.txt`; ordinary benchmarks never regenerate it.

Compare the existing history stack against the same stack with LMR using the frozen corpus, then play paired matches with the same checkpoint at 100 ms per move:

```powershell
# From build/bin
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/lmr-depth8.csv --lmr
./suiteBench.exe 10 ../../benchmarks/positions.txt ../../benchmarks/lmr-depth10.csv --lmr
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 100 --lmr
```

In `suiteBench --lmr` mode, `history` is the baseline and `lmr` is the candidate. CSV results include score differences and reduction/re-search counters; score differences are recorded rather than treated as ordering-test failures. The geometric ratio uses history nodes / LMR nodes. In `headlessMatch --lmr`, only player A uses LMR; B keeps full-depth search. The game count must be even, and each seeded eight-turn opening is reused with colors reversed. Games run to the existing terminal/draw rules without evaluation adjudication. [LMR measurements and match results](benchmarks/lmr-results.md) include the limits of the strength evidence.

Both comparison tools also accept `--lmr-late` (start reductions at the seventh ordered move) or `--lmr-endgame` (keep the fifth-move threshold, but exempt nodes with five or fewer pieces). These change one condition at a time; all other LMR guards and full-depth verification stay the same. The optional final constructor argument is `LMRPolicy::Conservative`, `LMRPolicy::Late`, or `LMRPolicy::EndgameGuard`.

Suite CSVs include reductions, re-searches, accepted reduced cutoffs, confirmed re-search cutoffs, and work for reduced/re-searched calls at nodes with five or fewer pieces. A reduced result above alpha must be re-searched, so accepted reduced cutoffs are always zero. Work counters include descendants and can overlap when reduced calls nest; do not add them to total nodes. These counters cover endgame nodes reached from every position category, rather than only endgame roots. [Endgame diagnosis and controlled tuning](benchmarks/lmr-tuning-results.md) record the comparisons.

Use `--only-history`, `--only-lmr`, or `--only-lmr-endgame` to measure one policy per fresh `suiteBench` process. These modes label ratios `ratio_vs_self`; compare the separate CSVs to calculate baseline ratios and score differences. CSVs also include wall-clock NPS and CPU milliseconds (main-thread user plus kernel time on Windows, process CPU time elsewhere). CPU counters have coarse resolution for very short searches.

`headlessMatch` accepts optional fixed openings and per-search timing output after the policy flag. Each opening line contains hexadecimal encoded legal moves from the initial board; blank lines and `#` comments are ignored. Openings must be unique, playable, and end at a complete turn. The game count must equal twice the opening count. `--check-openings <file>` validates a fixture without loading models or tablebases. `--lmr-vs-original` compares guarded LMR as A against original LMR as B. The other LMR flags retain full-depth search as B; omitting the opening file retains the original seeded eight-turn openings.

```sh
cd build/bin
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/single-baseline.csv --only-history
./suiteBench.exe 8 ../../benchmarks/positions.txt ../../benchmarks/single-guard.csv --only-lmr-endgame
./headlessMatch.exe --check-openings ../../benchmarks/strength-openings.txt
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 200 --lmr ../../benchmarks/strength-openings.txt ../../benchmarks/original-moves.csv
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 200 --lmr-endgame ../../benchmarks/strength-openings.txt ../../benchmarks/guard-moves.csv
./headlessMatch.exe nnue_best_v2.bin nnue_best_v2.bin 200 --lmr-vs-original ../../benchmarks/strength-openings.txt ../../benchmarks/direct-moves.csv
```

Timing CSVs record every `AIPlayer::search` call's actual elapsed time and nominal 100 ms budget, including PV completion, alongside material, move category, nodes, and completed depth. The console reports per-player mean, median, p95, and maximum elapsed search time. [Timing replication, suite uniqueness, and expanded matches](benchmarks/lmr-validation-results.md) retain the measurements and protocol.

Move ordering, opt-in LMR, and the benchmark validation in this change were made by 6.1 Sol.

**Web (local):**
```bash
cd docs && python3 -m http.server 8000
# open http://localhost:8000
```

A local HTTP server is required — browsers block WebAssembly from file URLs.
