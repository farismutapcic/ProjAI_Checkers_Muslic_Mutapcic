#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <limits>
#include <functional>
#include <atomic>
#include <cmath>   // std::abs
#include <utility> // std::move
#include <algorithm>

#include <gui/Image.h>
#include <gui/Sound.h>
#include <td/Types.h>

static constexpr int N = 8;

// ------------------------------
// DRAW RULE SETTINGS
// ------------------------------
// 30 "full moves" (red + black) without capture => 60 plies.
// If you want "30 plies total" instead, set DRAW_PLY_LIMIT = 30.
static constexpr int DRAW_FULL_MOVES_NO_CAPTURE = 30;
static constexpr int DRAW_PLY_LIMIT = DRAW_FULL_MOVES_NO_CAPTURE * 2;

class Checkers {
public:
    using INT = td::INT2;
    using Piece = int; // 0 empty, 1 red man, 2 red king, -1 black man, -2 black king

    enum class GameResult {
        None,
        Win,
        Lose,
        Draw
    };

    struct Cell {
        INT r, c;
        Cell() : r(-1), c(-1) {}
        Cell(INT R, INT C) : r(R), c(C) {}
        bool operator==(const Cell& o) const { return r == o.r && c == o.c; }
        bool valid() const { return r >= 0 && r < N && c >= 0 && c < N; }
    };

    struct Move {
        std::vector<Cell> seq;
        bool isCapture() const {
            if (seq.size() < 2) return false;
            for (size_t i = 1; i < seq.size(); ++i) {
                if (std::abs((int)seq[i].r - (int)seq[i - 1].r) == 2) return true;
            }
            return seq.size() > 2;
        }
    };

    struct State {
        Piece b[N][N];
        bool redToMove;
        State() { clear(); }
        void clear() {
            for (int r = 0; r < N; ++r)
                for (int c = 0; c < N; ++c)
                    b[r][c] = 0;
            redToMove = true;
        }
    };

private:
    // resources
    gui::Image _imgBoard;
    gui::Image _imgRedMan;
    gui::Image _imgRedKing;
    gui::Image _imgBlackMan;
    gui::Image _imgBlackKing;
    gui::Sound _sndMove;
    gui::Sound _sndWin;

    // overlays
    gui::Image _imgSelHL;   // :hl_sel
    gui::Image _imgMoveHL;  // :hl_move

    // board placement
    gui::Rect _boardPlacement;
    td::Point<td::INT2> _origin;
    td::Size<td::INT2> _cellSize;
    const float MARGIN = 0.03f;

    // game
    mutable std::mutex _mtx;
    State _st;
    bool _playing{false};

    // result + draw counter
    GameResult _result{GameResult::None};
    int _noCapturePlies{0}; // counts plies without capture; when reaches DRAW_PLY_LIMIT => draw

    // input (selected piece)
    Cell _clickFrom;
    std::vector<Cell> _legalTargets;

    // animation
    struct Anim {
        bool active{false};
        Move mv;
        size_t idx{0};      // segment idx -> idx+1
        float t{0.0f};
        float dur{0.14f};   // duration per segment
        Piece movingPiece{0};
        Cell curFrom;
        Cell curTo;
        bool moveHadCapture{false}; // ✅ capture info for whole move
    } _anim;

    // flags so View can react
    bool _moveStartedFlag{false};
    bool _moveFinishedFlag{false};

    // AI
    int _aiDepth{6};
    std::thread _worker;
    std::atomic<bool> _aiRunning{false};
    std::atomic<bool> _stopAi{false};
    bool _aiPlaysRed{false}; // default: user is red, AI is black

    struct SimpleLock {
        std::mutex& m;
        explicit SimpleLock(std::mutex& mm) : m(mm) { m.lock(); }
        ~SimpleLock() { m.unlock(); }
        SimpleLock(const SimpleLock&) = delete;
        SimpleLock& operator=(const SimpleLock&) = delete;
    };

private:
    // ---------- "unsafe" helpers (CALL ONLY WHEN _mtx IS ALREADY LOCKED) ----------
    bool isPlayingUnsafe() const { return _playing; }
    bool isRedTurnUnsafe() const { return _st.redToMove; }

    bool isAITurnUnsafe() const {
        if (!_playing) return false;
        return (_st.redToMove == _aiPlaysRed);
    }

    bool isUserTurnUnsafe() const {
        if (!_playing) return false;
        return (_st.redToMove != _aiPlaysRed);
    }

    static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

    static gui::Rect lerpRect(const gui::Rect& a, const gui::Rect& b, float t) {
        gui::Rect r;
        r.left   = lerpf(a.left,   b.left,   t);
        r.right  = lerpf(a.right,  b.right,  t);
        r.bottom = lerpf(a.bottom, b.bottom, t);
        r.top    = lerpf(a.top,    b.top,    t);
        return r;
    }

    static float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

    void drawPieceAtRect(Piece p, const gui::Rect& rc) {
        if (p == 1) _imgRedMan.draw(rc);
        else if (p == 2) _imgRedKing.draw(rc);
        else if (p == -1) _imgBlackMan.draw(rc);
        else if (p == -2) _imgBlackKing.draw(rc);
    }

    void clearSelectionUnsafe() {
        _clickFrom = Cell();
        _legalTargets.clear();
    }

    void recomputeLegalTargetsUnsafe() {
        _legalTargets.clear();
        if (!_clickFrom.valid()) return;

        auto moves = legalMoves(_st);
        for (const auto& m : moves) {
            if (m.seq.size() < 2) continue;
            if (m.seq.front() == _clickFrom) {
                _legalTargets.push_back(m.seq.back());
            }
        }
    }

    // Determine Win/Lose relative to USER (user is opposite of AI color)
    void setWinLoseForSideToMoveHasNoMovesUnsafe() {
        // If side to move has no moves -> that side loses.
        // userIsRed = !aiPlaysRed
        const bool userIsRed = !_aiPlaysRed;
        const bool redToMove = _st.redToMove;

        const bool userToMove = (redToMove == userIsRed);
        if (userToMove) _result = GameResult::Lose;
        else _result = GameResult::Win;

        _playing = false;
    }

    void checkEndConditionsUnsafe() {
        if (!_playing) return;

        // 1) DRAW by 30 full moves without capture
        if (_noCapturePlies >= DRAW_PLY_LIMIT) {
            _result = GameResult::Draw;
            _playing = false;
            return;
        }

        // 2) No legal moves -> loss for side to move
        auto moves = legalMoves(_st);
        if (moves.empty()) {
            setWinLoseForSideToMoveHasNoMovesUnsafe();
            return;
        }
    }

    void startAnimMoveUnsafe(const Move& m) {
        if (m.seq.size() < 2) return;
        if (_anim.active) return;
        if (!_playing) return;

        _anim.active = true;
        _anim.mv = m;
        _anim.idx = 0;
        _anim.t = 0.0f;

        _anim.curFrom = m.seq[0];
        _anim.curTo   = m.seq[1];

        _anim.movingPiece = _st.b[_anim.curFrom.r][_anim.curFrom.c];
        _anim.moveHadCapture = m.isCapture();

        // remove piece from start so it won't be drawn twice
        _st.b[_anim.curFrom.r][_anim.curFrom.c] = 0;

        _moveStartedFlag = true;
    }

    // finish ONE segment (curFrom -> curTo)
    void finishAnimSegmentUnsafe() {
        const Cell a = _anim.curFrom;
        const Cell b = _anim.curTo;

        // capture jump -> remove jumped piece immediately
        if (std::abs((int)b.r - (int)a.r) == 2 && std::abs((int)b.c - (int)a.c) == 2) {
            int mr = ((int)a.r + (int)b.r) / 2;
            int mc = ((int)a.c + (int)b.c) / 2;
            _st.b[mr][mc] = 0;
        }

        // place piece on destination of this segment
        _st.b[b.r][b.c] = _anim.movingPiece;

        // advance to next segment if exists
        _anim.idx++;
        if (_anim.idx + 1 < _anim.mv.seq.size()) {
            // remove it again so we animate next segment cleanly
            _st.b[b.r][b.c] = 0;

            _anim.curFrom = _anim.mv.seq[_anim.idx];
            _anim.curTo   = _anim.mv.seq[_anim.idx + 1];
            _anim.t = 0.0f;
            return;
        }

        // last segment finished: finalize (promotion + turn flip)
        Piece finalPiece = _anim.movingPiece;
        if (finalPiece == 1 && b.r == 0) finalPiece = 2;
        if (finalPiece == -1 && b.r == N - 1) finalPiece = -2;
        _st.b[b.r][b.c] = finalPiece;

        // ✅ update draw counter: reset on capture, increment otherwise (per ply)
        if (_anim.moveHadCapture) _noCapturePlies = 0;
        else _noCapturePlies++;

        // flip turn
        _st.redToMove = !_st.redToMove;

        // clear animation + selection
        _anim = Anim{};
        clearSelectionUnsafe();
        _moveFinishedFlag = true;

        // check draw / win-lose
        checkEndConditionsUnsafe();
    }

public:
    explicit Checkers(int aiDepth = 6)
        : _imgBoard(":board")
        , _imgRedMan(":rm")
        , _imgRedKing(":rk")
        , _imgBlackMan(":bm")
        , _imgBlackKing(":bk")
        , _sndMove(":move")
        , _sndWin(":success")
        , _imgSelHL(":hl_sel")
        , _imgMoveHL(":hl_move")
        , _aiDepth(aiDepth)
    {
        init();
    }

    ~Checkers() { stop(); }

    void setAIPlaysRed(bool v) { _aiPlaysRed = v; }

    GameResult getResult() const {
        SimpleLock lg(_mtx);
        return _result;
    }

    void clearResult() {
        SimpleLock lg(_mtx);
        _result = GameResult::None;
    }

    // ---------- lifecycle ----------
    void init() {
        SimpleLock lg(_mtx);
        _st.clear();

        // black at top
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < N; ++c)
                if ((r + c) % 2 == 1) _st.b[r][c] = -1;

        // red at bottom
        for (int r = N - 3; r < N; ++r)
            for (int c = 0; c < N; ++c)
                if ((r + c) % 2 == 1) _st.b[r][c] = 1;

        _st.redToMove = true;
        _playing = false;

        _result = GameResult::None;
        _noCapturePlies = 0;

        clearSelectionUnsafe();
        _anim = Anim{};
        _moveStartedFlag = false;
        _moveFinishedFlag = false;
    }

    void reset() {
        stopAI();
        init();
    }

    void start() {
        stopAI();
        SimpleLock lg(_mtx);
        _playing = true;

        _result = GameResult::None;
        _noCapturePlies = 0;

        clearSelectionUnsafe();
        _anim = Anim{};
        _moveStartedFlag = false;
        _moveFinishedFlag = false;

        // if someone has no moves at start (shouldn't), handle
        checkEndConditionsUnsafe();
    }

    void stop() {
        stopAI();
        SimpleLock lg(_mtx);
        _playing = false;
        clearSelectionUnsafe();
        _anim = Anim{};
        _moveStartedFlag = false;
        _moveFinishedFlag = false;
    }

    bool isPlaying() const {
        SimpleLock lg(_mtx);
        return isPlayingUnsafe();
    }

    bool isRedTurn() const {
        SimpleLock lg(_mtx);
        return isRedTurnUnsafe();
    }

    bool isAITurn() const {
        SimpleLock lg(_mtx);
        return isAITurnUnsafe();
    }

    bool isUserTurn() const {
        SimpleLock lg(_mtx);
        return isUserTurnUnsafe();
    }

    bool isAIRunning() const { return _aiRunning.load(); }

    bool isAnimating() const {
        SimpleLock lg(_mtx);
        return _anim.active;
    }

    // View calls these to react (sound/menu/AI)
    bool consumeMoveStarted() {
        SimpleLock lg(_mtx);
        bool v = _moveStartedFlag;
        _moveStartedFlag = false;
        return v;
    }

    bool consumeMoveFinished() {
        SimpleLock lg(_mtx);
        bool v = _moveFinishedFlag;
        _moveFinishedFlag = false;
        return v;
    }

    // ---------- geometry ----------
    void updateModelSize(const gui::Size& sz) {
        const float W = (float)sz.width;
        const float H = (float)sz.height;
        const float usableW = W * (1.0f - 2.0f * MARGIN);
        const float usableH = H * (1.0f - 2.0f * MARGIN);
        const float side = (usableW < usableH) ? usableW : usableH;
        const float left = (W - side) * 0.5f;
        const float bottom = (H - side) * 0.5f;

        _boardPlacement.left = left;
        _boardPlacement.bottom = bottom;
        _boardPlacement.right = left + side;
        _boardPlacement.top = bottom + side;

        const float cell = side / (float)N;
        _cellSize.width = (td::INT2)cell;
        _cellSize.height = (td::INT2)cell;
        _origin.x = (td::INT2)_boardPlacement.left;
        _origin.y = (td::INT2)_boardPlacement.bottom;
    }

private:
    // Board state: row 0 is TOP. Screen coordinates: origin is BOTTOM.
    static int rowToScreenY(int row) { return (N - 1) - row; }

public:
    // Y flip
    gui::Rect getCellRect(const Cell& pos) const {
        gui::Rect r;
        r.left = _origin.x + pos.c * _cellSize.width;

        const int sy = rowToScreenY((int)pos.r);
        r.bottom = _origin.y + sy * _cellSize.height;

        r.right = r.left + _cellSize.width;
        r.top = r.bottom + _cellSize.height;
        return r;
    }

    // Y flip back
    Cell cellFromPoint(const gui::Point& p) const {
        if (p.x < _boardPlacement.left || p.x > _boardPlacement.right ||
            p.y < _boardPlacement.bottom || p.y > _boardPlacement.top) {
            return Cell();
        }

        const int cx = (int)((p.x - _origin.x) / ((float)_cellSize.width));
        const int cy = (int)((p.y - _origin.y) / ((float)_cellSize.height));
        if (cx < 0 || cx >= N || cy < 0 || cy >= N) return Cell();

        const int row = (N - 1) - cy;
        return Cell((INT)row, (INT)cx);
    }

    // ---------- animation tick ----------
    void update(float dtSeconds) {
        SimpleLock lg(_mtx);
        if (!_anim.active) return;

        _anim.t += dtSeconds;
        if (_anim.t >= _anim.dur) {
            finishAnimSegmentUnsafe();
        }
    }

    // ---------- drawing ----------
    void draw() {
        _imgBoard.draw(_boardPlacement, gui::Image::AspectRatio::Keep,
                       td::HAlignment::Center, td::VAlignment::Center);

        SimpleLock lg(_mtx);

        // legal target highlights
        for (const auto& t : _legalTargets) {
            if (!t.valid()) continue;
            gui::Rect rc = getCellRect(t);
            _imgMoveHL.draw(rc);
        }

        // selected piece highlight
        if (_clickFrom.valid()) {
            gui::Rect rc = getCellRect(_clickFrom);
            _imgSelHL.draw(rc);
        }

        // draw all pieces
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                Piece p = _st.b[r][c];
                if (p == 0) continue;
                gui::Rect rc = getCellRect(Cell((INT)r, (INT)c));
                drawPieceAtRect(p, rc);
            }
        }

        // moving piece on top
        if (_anim.active && _anim.movingPiece != 0) {
            float t = _anim.t / _anim.dur;
            if (t < 0) t = 0;
            if (t > 1) t = 1;

            float u = smoothstep(t);

            gui::Rect a = getCellRect(_anim.curFrom);
            gui::Rect b = getCellRect(_anim.curTo);
            gui::Rect rc = lerpRect(a, b, u);
            drawPieceAtRect(_anim.movingPiece, rc);
        }
    }

private:
    // ---------- move generation ----------
    bool inside(int r, int c) const { return r >= 0 && r < N && c >= 0 && c < N; }

    void captureDFS(State s, int r, int c, std::vector<Cell>& path,
                    std::vector<Move>& out, Piece piece) const
    {
        const bool isKing = std::abs(piece) == 2;
        const bool isRed = piece > 0;
        const int dirKing[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
        const int dirRed[2][2] = { {-1,1},{-1,-1} };
        const int dirBlack[2][2] = { {1,1},{1,-1} };
        const int (*dirs)[2] = isKing ? dirKing : (isRed ? dirRed : dirBlack);
        const int nd = isKing ? 4 : 2;

        bool found = false;
        for (int i = 0; i < nd; ++i) {
            const int mr = r + dirs[i][0];
            const int mc = c + dirs[i][1];
            const int jr = r + 2 * dirs[i][0];
            const int jc = c + 2 * dirs[i][1];
            if (!inside(mr, mc) || !inside(jr, jc)) continue;
            if (s.b[mr][mc] == 0) continue;
            if ((s.b[mr][mc] > 0) == (piece > 0)) continue;
            if (s.b[jr][jc] != 0) continue;

            State ns = s;
            ns.b[r][c] = 0;
            ns.b[mr][mc] = 0;

            Piece placed = piece;
            if (!isKing) {
                if ((piece == 1 && jr == 0) || (piece == -1 && jr == N - 1)) {
                    placed = (piece > 0) ? 2 : -2;
                    ns.b[jr][jc] = placed;
                    path.push_back(Cell((INT)jr, (INT)jc));
                    Move mv; mv.seq = path; out.push_back(mv);
                    path.pop_back();
                    found = true;
                    continue;
                }
            }

            ns.b[jr][jc] = placed;
            path.push_back(Cell((INT)jr, (INT)jc));
            captureDFS(ns, jr, jc, path, out, placed);
            path.pop_back();
            found = true;
        }

        if (!found && path.size() >= 2) {
            Move m; m.seq = path; out.push_back(m);
        }
    }

    std::vector<Move> genCaptures(const State& s) const {
        std::vector<Move> res;
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                Piece p = s.b[r][c];
                if (p == 0) continue;
                if ((s.redToMove && p > 0) || (!s.redToMove && p < 0)) {
                    std::vector<Cell> path; path.push_back(Cell((INT)r, (INT)c));
                    captureDFS(s, r, c, path, res, p);
                }
            }
        }
        return res;
    }

    std::vector<Move> genSimple(const State& s) const {
        std::vector<Move> res;
        const int dirKing[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
        const int dirRed[2][2] = { {-1,1},{-1,-1} };
        const int dirBlack[2][2] = { {1,1},{1,-1} };

        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                Piece p = s.b[r][c];
                if (p == 0) continue;
                if ((s.redToMove && p > 0) || (!s.redToMove && p < 0)) {
                    const bool isKing = std::abs(p) == 2;
                    const bool isRed = p > 0;
                    const int (*dirs)[2] = isKing ? dirKing : (isRed ? dirRed : dirBlack);
                    const int nd = isKing ? 4 : 2;
                    for (int i = 0; i < nd; ++i) {
                        int tr = r + dirs[i][0];
                        int tc = c + dirs[i][1];
                        if (!inside(tr, tc)) continue;
                        if (s.b[tr][tc] != 0) continue;
                        Move m; m.seq = { Cell((INT)r, (INT)c), Cell((INT)tr, (INT)tc) };
                        res.push_back(m);
                    }
                }
            }
        }
        return res;
    }

    std::vector<Move> legalMoves(const State& s) const {
        auto caps = genCaptures(s);
        if (!caps.empty()) return caps;
        return genSimple(s);
    }

    State stateAfter(const State& s, const Move& m) const {
        State ns = s;
        applyMoveUnsafe(ns, m);
        return ns;
    }

    void applyMoveUnsafe(State& s, const Move& m) const {
        if (m.seq.size() < 2) return;
        int sr = m.seq[0].r, sc = m.seq[0].c;
        Piece piece = s.b[sr][sc];
        s.b[sr][sc] = 0;

        for (size_t i = 1; i < m.seq.size(); ++i) {
            int tr = m.seq[i].r, tc = m.seq[i].c;
            if (std::abs(tr - sr) == 2 && std::abs(tc - sc) == 2) {
                int mr = (sr + tr) / 2;
                int mc = (sc + tc) / 2;
                s.b[mr][mc] = 0;
            }
            sr = tr; sc = tc;
        }

        if (piece == 1 && sr == 0) piece = 2;
        if (piece == -1 && sr == N - 1) piece = -2;
        s.b[sr][sc] = piece;
        s.redToMove = !s.redToMove;
    }

    int evalState(const State& s) const {
        int sc = 0;
        for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
            int v = s.b[r][c];
            if (v == 1) sc += 100 + (7 - r) * 3;
            if (v == 2) sc += 200;
            if (v == -1) sc -= 100 + r * 3;
            if (v == -2) sc -= 200;
        }
        State tmp = s; tmp.redToMove = true;
        int redMoves = (int)legalMoves(tmp).size();
        tmp.redToMove = false;
        int blackMoves = (int)legalMoves(tmp).size();
        sc += (redMoves - blackMoves) * 3;
        return sc;
    }

    int alphaBeta(const State& s, int depth, int alpha, int beta, bool maximizing) const {
        auto moves = legalMoves(s);
        if (moves.empty()) return s.redToMove ? -1000000 : 1000000;
        if (depth == 0) return evalState(s);

        if (maximizing) {
            int best = std::numeric_limits<int>::min();
            for (const auto& mv : moves) {
                State ns = stateAfter(s, mv);
                int v = alphaBeta(ns, depth - 1, alpha, beta, false);
                best = std::max(best, v);
                alpha = std::max(alpha, v);
                if (alpha >= beta) break;
            }
            return best;
        } else {
            int best = std::numeric_limits<int>::max();
            for (const auto& mv : moves) {
                State ns = stateAfter(s, mv);
                int v = alphaBeta(ns, depth - 1, alpha, beta, true);
                best = std::min(best, v);
                beta = std::min(beta, v);
                if (alpha >= beta) break;
            }
            return best;
        }
    }

public:
    // ---------- AI ----------
    void startAI(std::function<void()> onDone = nullptr) {
        State root;
        {
            SimpleLock lg(_mtx);
            if (!_playing) return;
            if (_anim.active) return;
            if (!isAITurnUnsafe()) return;
        }

        stopAI();
        _stopAi.store(false);
        _aiRunning.store(true);

        {
            SimpleLock lg(_mtx);
            root = _st;
        }

        _worker = std::thread([this, root, onDone]() {
            State cur = root;
            auto moves = legalMoves(cur);
            if (moves.empty()) {
                _aiRunning.store(false);
                if (onDone) onDone();
                return;
            }

            const bool maximizing = cur.redToMove;
            int bestVal = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
            Move best = moves[0];

            for (auto& m : moves) {
                if (_stopAi.load()) break;
                State ns = stateAfter(cur, m);
                int v = alphaBeta(ns, _aiDepth - 1,
                                  std::numeric_limits<int>::min() / 4,
                                  std::numeric_limits<int>::max() / 4,
                                  !maximizing);
                if (maximizing) {
                    if (v > bestVal) { bestVal = v; best = m; }
                } else {
                    if (v < bestVal) { bestVal = v; best = m; }
                }
            }

            {
                SimpleLock lg(_mtx);
                if (!_stopAi.load() && _playing && !_anim.active) {
                    startAnimMoveUnsafe(best);
                    clearSelectionUnsafe();
                }
            }

            _aiRunning.store(false);
            if (onDone) onDone();
        });
    }

    void stopAI() {
        _stopAi.store(true);
        if (_worker.joinable()) _worker.join();
        _aiRunning.store(false);
    }

    // ---------- USER INPUT (CLICK-CLICK) ----------
    void startUserClick(const gui::Point& p) {
        SimpleLock lg(_mtx);
        if (!_playing) return;
        if (_result != GameResult::None) return; // game ended
        if (_aiRunning.load()) return;
        if (_anim.active) return;
        if (!isUserTurnUnsafe()) return;

        const Cell cell = cellFromPoint(p);
        if (!cell.valid()) return;

        const bool userIsRed = !_aiPlaysRed;

        auto isUsersPiece = [&](Piece pc) {
            if (pc == 0) return false;
            return userIsRed ? (pc > 0) : (pc < 0);
        };

        // 1) nothing selected -> select user's piece
        if (!_clickFrom.valid()) {
            Piece pc = _st.b[cell.r][cell.c];
            if (!isUsersPiece(pc)) return;
            _clickFrom = cell;
            recomputeLegalTargetsUnsafe();
            return;
        }

        // 2) already selected: deselect
        if (cell == _clickFrom) {
            clearSelectionUnsafe();
            return;
        }

        // 3) click another own piece -> change selection
        {
            Piece pc = _st.b[cell.r][cell.c];
            if (isUsersPiece(pc)) {
                _clickFrom = cell;
                recomputeLegalTargetsUnsafe();
                return;
            }
        }

        // 4) try a move to clicked cell
        auto moves = legalMoves(_st);
        for (const auto& m : moves) {
            if (m.seq.size() < 2) continue;
            if (m.seq.front() == _clickFrom && m.seq.back() == cell) {
                startAnimMoveUnsafe(m);
                clearSelectionUnsafe();
                return;
            }
        }
        // invalid target: keep selection
    }

    bool finishUserClick(const gui::Point& /*p*/) { return false; }
};
