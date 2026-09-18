#pragma once

#include <gui/Canvas.h>
#include <gui/Sound.h>
#include <gui/Alert.h>
#include <gui/Timer.h>
#include <gui/InputDevice.h>

#include <functional>
#include "Checkers.h"

class ViewCheckersBoard : public gui::Canvas
{
protected:
    Checkers _engine;
    gui::Sound _soundMove;
    gui::Sound _soundSuccess;
    gui::Size _size;
    std::function<void()> _fnUpdateMenuAndTB;

    bool _gameOverDialogShown = false;

    // Timer-based animation loop
    gui::Timer _timer;
    bool _timerRunning = false;

    // defer game-over dialog (macOS safe)
    bool _pendingGameOver = false;
    Checkers::GameResult _pendingGR = Checkers::GameResult::None;

    void setAnimLoop(bool on)
    {
        if (on)
        {
            if (_timerRunning) return;
            _timerRunning = true;
            _timer.start();
        }
        else
        {
            if (!_timerRunning) return;
            _timerRunning = false;
            _timer.stop();
        }
    }

    void showGameFinishedDialog(Checkers::GameResult gr)
    {
        td::String title;
        td::String text;

        if (gr == Checkers::GameResult::Win)
        {
            title = tr("YouWin");
            text  = tr("YouWinInfo");
        }
        else if (gr == Checkers::GameResult::Lose)
        {
            title = tr("YouLose");
            text  = tr("YouLoseInfo");
        }
        else
        {
            title = tr("Draw");
            text  = tr("DrawInfo");
        }

        gui::Alert::showYesNoQuestion(
            title,
            text,
            tr("PlayAgain"),
            tr("Close"),
            [this](gui::Alert::Answer answer)
            {
                _engine.clearResult();
                _gameOverDialogShown = false;

                if (answer == gui::Alert::Answer::Yes)
                    this->reset();
            }
        );
    }

    // macOS safe: only queue, do NOT show inside onDraw
    void queueGameOverIfNeeded()
    {
        if (_gameOverDialogShown || _pendingGameOver) return;

        auto gr = _engine.getResult();
        if (gr == Checkers::GameResult::None) return;

        _pendingGameOver = true;
        _pendingGR = gr;

        // make sure timer is running so we can show it on next tick
        setAnimLoop(true);
    }

    // Called from timer (event loop) → safe to show modal alert on macOS
    void processPendingUI()
    {
        if (!_pendingGameOver) return;

        _pendingGameOver = false;
        _gameOverDialogShown = true;

        // stop animation loop before modal dialog (important on macOS)
        setAnimLoop(false);

        _soundSuccess.play();
        showGameFinishedDialog(_pendingGR);
    }

public:
    ViewCheckersBoard(const std::function<void()>& fnUpdateMenuAndTB)
        : gui::Canvas({ gui::InputDevice::Event::Keyboard, gui::InputDevice::Event::PrimaryClicks })
        , _engine(6)
        , _soundMove(":move")
        , _soundSuccess(":success")
        , _fnUpdateMenuAndTB(fnUpdateMenuAndTB)
        , _timer(this, 1.0f / 60.0f, false) // 60 FPS, does not start immediately
    {
        _timer.onTimer([this]() {
            // ✅ show pending alert outside onDraw
            processPendingUI();
            // continue redraw requests (if anim loop is on)
            reDraw();
        });

        setPreferredFrameRateRange(60, 60);
        enableResizeEvent(true);
        _engine.setAIPlaysRed(false);
    }

    void onResize(const gui::Size& newSize) override
    {
        _size = newSize;
        _engine.updateModelSize(newSize);
        reDraw();
    }

    void onDraw(const gui::Rect& /*rect*/) override
    {
        const float dt = 1.0f / 60.0f;

        _engine.update(dt);
        _engine.draw();

        if (_engine.consumeMoveStarted())
        {
            _soundMove.play();
            setAnimLoop(true);
        }

        if (_engine.consumeMoveFinished())
        {
            if (_fnUpdateMenuAndTB) _fnUpdateMenuAndTB();

            // queue game over (do not show here)
            queueGameOverIfNeeded();

            // AI move only if game still running
            if (_engine.isPlaying() && _engine.isAITurn() && !_engine.isAIRunning() && !_engine.isAnimating())
            {
                _engine.startAI([this]() { reDraw(); });
                setAnimLoop(true);
            }
        }

        // when nothing animates and AI not running → stop loop (unless we need to show dialog)
        if (!_engine.isAnimating() && !_engine.isAIRunning())
        {
            queueGameOverIfNeeded();

            if (!_pendingGameOver)
                setAnimLoop(false);
        }
    }

    void onPrimaryButtonPressed(const gui::InputDevice& inputDevice) override
    {
        const gui::Point& mp = inputDevice.getModelPoint();
        _engine.startUserClick(mp);

        reDraw();

        if (_engine.isAnimating())
            setAnimLoop(true);

        if (_fnUpdateMenuAndTB) _fnUpdateMenuAndTB();
    }

    void onPrimaryButtonReleased(const gui::InputDevice& /*inputDevice*/) override
    {
        // click-click mode: nothing
    }

    void stop()
    {
        _engine.stop();
        _engine.clearResult();
        _gameOverDialogShown = false;

        _pendingGameOver = false;
        _pendingGR = Checkers::GameResult::None;

        setAnimLoop(false);
        if (_fnUpdateMenuAndTB) _fnUpdateMenuAndTB();
        reDraw();
    }

    bool isPlaying() const { return _engine.isPlaying(); }

    void reset()
    {
        _engine.clearResult();
        _gameOverDialogShown = false;

        _pendingGameOver = false;
        _pendingGR = Checkers::GameResult::None;

        if (_engine.isAIRunning())
        {
            // if your framework supports this:
            gui::Sound::play(gui::Sound::Type::Beep);
            return;
        }

        _engine.reset();
        setAnimLoop(false);
        reDraw();
        if (_fnUpdateMenuAndTB) _fnUpdateMenuAndTB();
    }

    void startStop()
    {
        if (_engine.isPlaying())
        {
            stop();
            return;
        }

        _engine.clearResult();
        _gameOverDialogShown = false;

        _pendingGameOver = false;
        _pendingGR = Checkers::GameResult::None;

        _engine.start();
        reDraw();
        if (_fnUpdateMenuAndTB) _fnUpdateMenuAndTB();

        if (_engine.isAITurn() && !_engine.isAIRunning() && !_engine.isAnimating())
        {
            _engine.startAI([this]() { reDraw(); });
            setAnimLoop(true);
        }
    }
};
