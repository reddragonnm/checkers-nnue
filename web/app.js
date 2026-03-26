const boardEl = document.getElementById("board");
const statusEl = document.getElementById("status");
const turnEl = document.getElementById("turn-indicator");
const resetDarkEl = document.getElementById("reset-dark");
const resetLightEl = document.getElementById("reset-light");
const flipBoardEl = document.getElementById("flip-board");
const thinkTimeEl = document.getElementById("think-time");
const thinkTimeValEl = document.getElementById("think-time-val");
const evalBarRefEl = document.getElementById("eval-bar-ref");

const statEvalEl = document.getElementById("stat-eval");
const statDepthEl = document.getElementById("stat-depth");
const statTimeEl = document.getElementById("stat-time");
const statNodesEl = document.getElementById("stat-nodes");

const evalFillEl = document.getElementById("eval-fill");
const evalTextEl = document.getElementById("eval-text");

const state = {
  selected: -1,
  legalTargets: [],
  busy: true,
  flipped: false,
  thinkTime: 1000,
  aiMovedFrom: -1,
  aiMovedTo: -1,
  captureSequence: [],
};

function wrap(name, returnType, argTypes) {
  return Module.cwrap(name, returnType, argTypes);
}

let api;

function updateEvalBar(score) {
  const displayScore = (score / 100).toFixed(1);
  evalTextEl.textContent = (score > 0 ? "+" : "") + displayScore;

  let percent;
  const currentTurn = api.getTurn();
  // We want the bar to show Light advantage (growing from bottom)
  if (currentTurn === 1) {
    // Light's turn
    percent = 50 + (score / 600) * 100;
  } else {
    // Dark's turn
    percent = 50 - (score / 600) * 100;
  }

  percent = Math.max(5, Math.min(95, percent));

  if (window.innerWidth <= 950) {
    evalFillEl.style.width = `${percent}%`;
    evalFillEl.style.height = `100%`;
  } else {
    evalFillEl.style.height = `${percent}%`;
    evalFillEl.style.width = `100%`;
  }
}

function updateStats() {
  const score = api.getLastScore();
  const depth = api.getLastDepth();
  const time = api.getLastTime();
  const nodes = api.getNodesHit();

  statEvalEl.textContent = (score / 100).toFixed(2);
  statDepthEl.textContent = depth;
  statTimeEl.textContent = `${time}ms`;
  statNodesEl.textContent = nodes.toLocaleString();

  updateEvalBar(score);
}

function squareClass(index) {
  const row = Math.floor(index / 8);
  const col = index % 8;
  return (row + col) % 2 === 0 ? "light" : "dark";
}

// Maps UI index to board index based on flip state
function getBoardIdx(uiIdx) {
  return state.flipped ? 63 - uiIdx : uiIdx;
}

function renderBoard() {
  boardEl.innerHTML = "";

  // Update eval bar container height to match board
  if (window.innerWidth > 950) {
    evalBarRefEl.style.height = `${boardEl.clientHeight}px`;
  }

  for (let i = 0; i < 64; i += 1) {
    const boardIdx = getBoardIdx(i);
    const square = document.createElement("button");
    square.type = "button";
    square.className = `square ${squareClass(boardIdx)}`;
    square.setAttribute("data-index", boardIdx);

    if (state.selected === boardIdx) square.classList.add("selected");
    if (state.legalTargets.includes(boardIdx)) square.classList.add("target");

    // Highlight AI move squares
    if (boardIdx === state.aiMovedFrom || boardIdx === state.aiMovedTo) {
      square.classList.add("ai-move-highlight");
    }

    const piece = api.getPieceAt(boardIdx);
    if (piece) {
      const token = document.createElement("div");
      token.className = `piece piece-${piece}`;
      square.appendChild(token);
    }

    square.addEventListener("click", () => onSquareClick(boardIdx));
    boardEl.appendChild(square);
  }

  const gameState = api.getGameState();
  const turn = api.getTurn();
  const humanSide = api.getHumanSide();

  if (gameState === 0) {
    statusEl.textContent = "Game Ongoing";
    turnEl.textContent = turn === humanSide ? "Your Move" : "AI Thinking...";
  } else {
    if (gameState === 1) statusEl.textContent = "You Win!";
    else if (gameState === 2) statusEl.textContent = "AI Wins!";
    else statusEl.textContent = "Draw!";
    turnEl.textContent = "Game Over";
  }
}

function detectMoveSequence(boardBefore, boardAfter) {
  // Detect all changes in the board state to build a complete move sequence
  const changes = { movedFrom: -1, movedTo: -1, captures: [] };

  for (let i = 0; i < 64; i++) {
    const pieceBefore = boardBefore[i];
    const pieceAfter = boardAfter[i];

    if (pieceBefore !== pieceAfter) {
      if (pieceBefore !== 0 && pieceAfter === 0) {
        // Piece left this square (either moved or was captured)
        // If this is an opponent piece, it was captured
        changes.movedFrom = i;
      } else if (pieceBefore === 0 && pieceAfter !== 0) {
        // Piece arrived at this square
        changes.movedTo = i;
      } else if (pieceBefore !== 0 && pieceAfter === 0) {
        // Another piece was removed (capture in sequence)
        if (i !== changes.movedFrom) {
          changes.captures.push(i);
        }
      }
    }
  }

  return changes;
}

async function maybeRunAi() {
  const gameState = api.getGameState();
  const turn = api.getTurn();
  const humanSide = api.getHumanSide();

  if (gameState !== 0 || turn === humanSide) {
    renderBoard();
    return;
  }

  state.busy = true;
  state.selected = -1;
  state.legalTargets = [];

  // Store board state before AI move to detect which squares changed
  const boardBefore = [];
  for (let i = 0; i < 64; i++) {
    boardBefore[i] = api.getPieceAt(i);
  }

  renderBoard();

  await new Promise((resolve) => setTimeout(resolve, 50));

  const moveResult = api.makeAiMove(state.thinkTime);

  // Get board state after move
  const boardAfter = [];
  for (let i = 0; i < 64; i++) {
    boardAfter[i] = api.getPieceAt(i);
  }

  const moveSequence = detectMoveSequence(boardBefore, boardAfter);
  state.aiMovedFrom = moveSequence.movedFrom;
  state.aiMovedTo = moveSequence.movedTo;
  state.captureSequence = moveSequence.captures;

  updateStats();
  state.busy = false;
  renderBoard();

  // Animate movement and capture sequence if there are multiple jumps
  if (state.captureSequence.length > 0) {
    for (let i = 0; i < state.captureSequence.length; i++) {
      setTimeout(
        () => {
          const square = boardEl.querySelector(
            `[data-index="${state.captureSequence[i]}"]`,
          );
          if (square) {
            square.classList.add("capture-animation");
          }
        },
        (i + 1) * 300,
      );
    }
  }

  // Animate piece movement
  setTimeout(() => {
    const piece = boardEl.querySelector(
      `[data-index="${state.aiMovedTo}"] .piece`,
    );
    if (piece) {
      piece.classList.add("piece-move-animation");
    }
  }, 100);

  // Clear highlights after a delay
  setTimeout(
    () => {
      state.aiMovedFrom = -1;
      state.aiMovedTo = -1;
      state.captureSequence = [];
      renderBoard();
    },
    1500 + state.captureSequence.length * 300,
  );

  if (api.getTurn() !== humanSide && api.getGameState() === 0) {
    maybeRunAi();
  }
}

async function onSquareClick(boardIdx) {
  if (state.busy || api.getGameState() !== 0) return;
  const humanSide = api.getHumanSide();
  if (api.getTurn() !== humanSide) return;

  if (state.selected !== -1 && state.legalTargets.includes(boardIdx)) {
    // Store board state before move
    const boardBefore = [];
    for (let i = 0; i < 64; i++) {
      boardBefore[i] = api.getPieceAt(i);
    }

    const result = api.makeHumanMove(state.selected, boardIdx);
    if (result === 0) return;

    // Get board state after move
    const boardAfter = [];
    for (let i = 0; i < 64; i++) {
      boardAfter[i] = api.getPieceAt(i);
    }

    if (result === 2) {
      // Multi-capture in progress - animate this leg of the sequence
      const moveSequence = detectMoveSequence(boardBefore, boardAfter);
      state.aiMovedFrom = moveSequence.movedFrom;
      state.aiMovedTo = moveSequence.movedTo;
      state.captureSequence = moveSequence.captures;

      state.selected = boardIdx;
      state.legalTargets = getLegalTargets(boardIdx);
      renderBoard();

      // Animate piece movement
      setTimeout(() => {
        const piece = boardEl.querySelector(
          `[data-index="${state.aiMovedTo}"] .piece`,
        );
        if (piece) {
          piece.classList.add("piece-move-animation");
        }
      }, 50);

      // Highlight captured squares
      if (state.captureSequence.length > 0) {
        for (let i = 0; i < state.captureSequence.length; i++) {
          setTimeout(
            () => {
              const square = boardEl.querySelector(
                `[data-index="${state.captureSequence[i]}"]`,
              );
              if (square) {
                square.classList.add("capture-animation");
              }
            },
            (i + 1) * 200,
          );
        }
      }
      return;
    }

    // Move is complete, animate final position
    const moveSequence = detectMoveSequence(boardBefore, boardAfter);
    state.aiMovedFrom = moveSequence.movedFrom;
    state.aiMovedTo = moveSequence.movedTo;
    state.captureSequence = moveSequence.captures;

    state.selected = -1;
    state.legalTargets = [];
    renderBoard();

    // Animate piece movement
    setTimeout(() => {
      const piece = boardEl.querySelector(
        `[data-index="${state.aiMovedTo}"] .piece`,
      );
      if (piece) {
        piece.classList.add("piece-move-animation");
      }
    }, 50);

    // Clear highlights after delay
    setTimeout(
      () => {
        state.aiMovedFrom = -1;
        state.aiMovedTo = -1;
        state.captureSequence = [];
        renderBoard();
      },
      800 + state.captureSequence.length * 200,
    );

    await maybeRunAi();
    return;
  }

  const piece = api.getPieceAt(boardIdx);
  const isOwnPiece =
    (humanSide === 0 && (piece === 1 || piece === 2)) ||
    (humanSide === 1 && (piece === 3 || piece === 4));

  if (!isOwnPiece) {
    state.selected = -1;
    state.legalTargets = [];
    renderBoard();
    return;
  }

  state.selected = boardIdx;
  state.legalTargets = getLegalTargets(boardIdx);
  renderBoard();
}

function getLegalTargets(from) {
  const count = api.getLegalMoveCountFrom(from);
  const targets = [];
  for (let i = 0; i < count; i += 1) {
    targets.push(api.getLegalMoveTo(from, i));
  }
  return targets;
}

function start() {
  api = {
    initEngine: wrap("init_engine", "number", []),
    resetGame: wrap("reset_game", null, ["number"]),
    getTurn: wrap("get_turn", "number", []),
    getHumanSide: wrap("get_human_side", "number", []),
    getGameState: wrap("get_game_state", "number", []),
    getPieceAt: wrap("get_piece_at", "number", ["number"]),
    getLegalMoveCountFrom: wrap("get_legal_move_count_from", "number", [
      "number",
    ]),
    getLegalMoveTo: wrap("get_legal_move_to", "number", ["number", "number"]),
    makeHumanMove: wrap("make_human_move", "number", ["number", "number"]),
    makeAiMove: wrap("make_ai_move", "number", ["number"]),
    getLastScore: wrap("get_last_score", "number", []),
    getLastDepth: wrap("get_last_depth", "number", []),
    getLastTime: wrap("get_last_time", "number", []),
    getNodesHit: wrap("get_nodes_hit", "number", []),
  };

  if (!api.initEngine()) {
    statusEl.textContent = "WASM Error";
    return;
  }

  state.busy = false;
  resetDarkEl.disabled = false;
  resetLightEl.disabled = false;
  flipBoardEl.disabled = false;

  resetDarkEl.addEventListener("click", () => {
    state.flipped = false;
    api.resetGame(0);
    state.selected = -1;
    state.legalTargets = [];
    state.aiMovedFrom = -1;
    state.aiMovedTo = -1;
    evalFillEl.style.transition = "none";
    // Initialize eval bar to 50%
    evalFillEl.style.height = "50%";
    evalTextEl.textContent = "0.0";
    setTimeout(() => {
      evalFillEl.style.transition = "height 0.5s cubic-bezier(0.4, 0, 0.2, 1)";
    }, 50);
    renderBoard();
    maybeRunAi();
  });

  resetLightEl.addEventListener("click", () => {
    state.flipped = true;
    api.resetGame(1);
    state.selected = -1;
    state.legalTargets = [];
    state.aiMovedFrom = -1;
    state.aiMovedTo = -1;
    evalFillEl.style.transition = "none";
    // Initialize eval bar to 50%
    evalFillEl.style.height = "50%";
    evalTextEl.textContent = "0.0";
    setTimeout(() => {
      evalFillEl.style.transition = "height 0.5s cubic-bezier(0.4, 0, 0.2, 1)";
    }, 50);
    renderBoard();
    maybeRunAi();
  });

  flipBoardEl.addEventListener("click", () => {
    state.flipped = !state.flipped;
    renderBoard();
    // Flip eval bar when board is flipped
    const currentHeight = evalFillEl.style.height || "50%";
    const heightValue = parseFloat(currentHeight);
    const flippedHeight = 100 - heightValue;
    evalFillEl.style.height = `${flippedHeight}%`;
  });

  thinkTimeEl.addEventListener("input", (e) => {
    state.thinkTime = parseInt(e.target.value);
    thinkTimeValEl.textContent = `${state.thinkTime}ms`;
  });

  renderBoard();
  // Re-render once more to ensure heights are correct after initial load
  setTimeout(renderBoard, 100);
}

window.addEventListener("resize", renderBoard);
window.addEventListener("wasm-ready", start);
