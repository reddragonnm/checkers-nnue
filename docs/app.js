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
};

function wrap(name, returnType, argTypes) {
  return Module.cwrap(name, returnType, argTypes);
}

let api;

function updateEvalBar(score) {
  const humanSide = api.getHumanSide(); // 0: Dark, 1: Light
  const aiSide = 1 - humanSide;

  // Requirement: "show the number in eval bar as always negative of ai evaluation"
  // If AI score is +0.6 (AI winning), display -0.6 (relative to Human/Absolute perspective).
  const displayValue = -score;
  const displayScore = (displayValue / 100).toFixed(1);
  evalTextEl.textContent = (displayValue > 0 ? "+" : "") + displayScore;

  // To calculate fill percentage (White/Light advantage), we need Light's absolute score.
  // AI score is relative to AI. 
  // If AI is Light (1), LightScore = score.
  // If AI is Dark (0), LightScore = -score.
  const lightScore = (aiSide === 1) ? score : -score;

  // whitePercent represents the White/Light advantage fill
  let whitePercent = 50 + (lightScore / 600) * 100;
  whitePercent = Math.max(5, Math.min(95, whitePercent));

  if (window.innerWidth <= 950) {
    evalFillEl.style.height = `100%`;
    evalFillEl.style.bottom = `0`;
    // Flipped logic for mobile (horizontal)
    if (!state.flipped) {
      evalFillEl.style.right = "";
      evalFillEl.style.left = "0";
      evalFillEl.style.width = `${whitePercent}%`;
    } else {
      evalFillEl.style.left = "";
      evalFillEl.style.right = "0";
      evalFillEl.style.width = `${whitePercent}%`;
    }
  } else {
    // Desktop: Vertical bar. 
    // Requirement: "vertically flip the eval bar" (White at top by default)
    evalFillEl.style.width = `100%`;
    evalFillEl.style.left = `0`;
    if (!state.flipped) {
      // White at Top
      evalFillEl.style.bottom = "";
      evalFillEl.style.top = "0";
      evalFillEl.style.height = `${whitePercent}%`;
    } else {
      // White at Bottom
      evalFillEl.style.top = "";
      evalFillEl.style.bottom = "0";
      evalFillEl.style.height = `${whitePercent}%`;
    }
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
    const square = document.createElement("div");
    square.className = `square ${squareClass(boardIdx)}`;
    square.setAttribute("data-index", boardIdx);

    if (state.selected === boardIdx) square.classList.add("selected");
    if (state.legalTargets.includes(boardIdx)) square.classList.add("target");

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
  renderBoard();

  await new Promise((resolve) => setTimeout(resolve, 300));

  const pvCount = api.makeAiMove(state.thinkTime);
  updateStats();

  if (pvCount > 0) {
    let moveResult = 2; // Start as multi-jump/initial
    while (moveResult === 2) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      moveResult = api.executeNextAiMove();
      renderBoard();
      if (moveResult === 0) break; // Error or no more moves
    }
  }

  state.busy = false;
  renderBoard();

  if (api.getTurn() !== humanSide && api.getGameState() === 0) {
    maybeRunAi();
  }
}

async function onSquareClick(boardIdx) {
  if (state.busy || api.getGameState() !== 0) return;
  const humanSide = api.getHumanSide();
  if (api.getTurn() !== humanSide) return;

  if (state.selected !== -1 && state.legalTargets.includes(boardIdx)) {
    const result = api.makeHumanMove(state.selected, boardIdx);
    if (result === 0) return;

    if (result === 2) {
      // Multi-capture in progress
      state.selected = boardIdx;
      state.legalTargets = getLegalTargets(boardIdx);
      renderBoard();
      
      // Delay before allowing next move in multi-capture
      state.busy = true;
      setTimeout(() => {
        state.busy = false;
      }, 500);
      return;
    }

    state.selected = -1;
    state.legalTargets = [];
    updateStats();
    renderBoard();

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
    executeNextAiMove: wrap("execute_next_ai_move", "number", []),
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
    evalFillEl.style.transition = "none";
    updateEvalBar(0);
    setTimeout(() => {
      evalFillEl.style.transition = "height 0.6s cubic-bezier(0.4, 0, 0.2, 1)";
    }, 50);
    renderBoard();
    maybeRunAi();
  });

  resetLightEl.addEventListener("click", () => {
    state.flipped = true;
    api.resetGame(1);
    state.selected = -1;
    state.legalTargets = [];
    evalFillEl.style.transition = "none";
    updateEvalBar(0);
    setTimeout(() => {
      evalFillEl.style.transition = "height 0.6s cubic-bezier(0.4, 0, 0.2, 1)";
    }, 50);
    renderBoard();
    maybeRunAi();
  });

  flipBoardEl.addEventListener("click", () => {
    state.flipped = !state.flipped;
    renderBoard();
    updateStats(); // Re-render eval bar with current perspective
  });

  thinkTimeEl.addEventListener("input", (e) => {
    state.thinkTime = parseInt(e.target.value);
    thinkTimeValEl.textContent = `${state.thinkTime}ms`;
  });

  // Initial eval bar state
  updateEvalBar(0);
  renderBoard();
  // Re-render once more to ensure heights are correct after initial load
  setTimeout(renderBoard, 100);
}

window.addEventListener("resize", renderBoard);
window.addEventListener("wasm-ready", start);
