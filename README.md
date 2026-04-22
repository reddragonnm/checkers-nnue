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

At depth zero, rather than returning the static evaluation immediately, the search continues to resolve all capture sequences. Stopping mid-capture would give wildly inaccurate evaluations. The quiescence search uses a stand-pat value — if the static evaluation already beats beta, it returns immediately — and only searches captures, not quiet moves. This keeps it bounded.

### Multi-Capture Handling

Multi-jump captures require special treatment throughout the search. When a move is made and the same player must continue capturing, the depth does not decrease and the ply does not increment — the continuation is treated as part of the same half-move. The principal variation is built to include the entire capture sequence up to the point where the turn finally switches.

### EGTB Integration

When the total number of pieces on the board drops to five or fewer, the search probes the endgame tablebase at every node (except the root). A tablebase win returns a score that reflects how quickly the win can be forced — shorter paths score higher. A tablebase loss returns a score that reflects how long the loss can be delayed — longer resistance scores higher. A tablebase draw returns zero, and the search continues normally from the root to find the best drawing move rather than returning immediately.

This design means the engine plays perfectly in all ≤5-piece endings and uses the tablebase to guide move selection even when the material is not yet reduced to tablebase range — a position that leads to a forced tablebase win in fewer moves scores higher than one that takes longer.

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

**Web (local):**
```bash
cd docs && python3 -m http.server 8000
# open http://localhost:8000
```

A local HTTP server is required — browsers block WebAssembly from file URLs.
