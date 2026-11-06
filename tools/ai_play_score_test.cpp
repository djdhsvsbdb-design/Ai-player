#include "mod/ai_play/score.hpp"

#include <cassert>
#include <limits>

int main() {
    using aiplay::score::ScoreBoard;
    ScoreBoard board;

    // Progress awards one point per newly reached whole percent, not per frame.
    board.beginAttempt();
    assert(board.observeProgress(0.4) == 0);
    assert(board.observeProgress(1.2) == 1);
    assert(board.observeProgress(1.9) == 0);
    assert(board.observeProgress(5.0) == 4);
    assert(board.points == 5);

    // A death applies one -10 penalty and cannot be charged twice.
    assert(board.recordDeath() == 5);
    assert(board.points == 0);
    assert(board.recordDeath() == 0);
    assert(!board.attemptActive);
    assert(board.observeProgress(80.0) == 0);

    // A later attempt can earn points again; a clear awards the remaining progress.
    board.beginAttempt();
    assert(board.observeProgress(37.8) == 37);
    board.finishClear();
    assert(board.points == 100);
    assert(!board.attemptActive);

    // Non-finite and out-of-range observations are safe.
    board.beginAttempt();
    assert(board.observeProgress(std::numeric_limits<double>::quiet_NaN()) == 0);
    assert(board.observeProgress(150.0) == 100);
    assert(board.points == 200);
    board.cancelAttempt();
    assert(board.observeProgress(100.0) == 0);
    return 0;
}
