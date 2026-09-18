#pragma once
#include <gui/Window.h>
#include "MenuBar.h"
#include "ToolBar.h"
#include "ViewCheckersBoard.h"
#include "Constants.h"
#include "DialogSettings.h"
#include <functional>

class MainWindow : public gui::Window
{
protected:
    gui::Image _imgStart;
    gui::Image _imgStop;

    MenuBar _mainMenuBar;
    ToolBar _toolBar;
    ViewCheckersBoard _mainView;

    const td::UINT4 _cSettingsDlgID = 17;

    void updateMenuAndTB()
    {
        bool isGamePlaying = _mainView.isPlaying();

        gui::MenuItem* pMenuItem = _mainMenuBar.getItem(cMenuGame, 0, 0, cStartStopActionItem);
        if (pMenuItem) pMenuItem->setChecked(isGamePlaying);

        gui::ToolBarItem* pTBItem = _toolBar.getItem(cMenuGame, 0, 0, cStartStopActionItem);
        if (pTBItem)
        {
            if (isGamePlaying)
            {
                pTBItem->setImage(&_imgStop);
                pTBItem->setLabel(tr("stop"));
                pTBItem->setTooltip(tr("stopTT"));
            }
            else
            {
                pTBItem->setImage(&_imgStart);
                pTBItem->setLabel(tr("start"));
                pTBItem->setTooltip(tr("startTT"));
            }
        }
    }

    bool onActionItem(gui::ActionItemDescriptor& aiDesc) override
    {
        auto [menuID, firstSub, lastSub, actionID] = aiDesc.getIDs();

        // SETTINGS
        if (menuID == cMenuApp && actionID == cSettingsActionItem)
        {
            auto pDlg = getAttachedWindow(_cSettingsDlgID);
            if (pDlg)
                pDlg->setFocus();
            else
            {
                auto* pSettingsDlg = new DialogSettings(this, _cSettingsDlgID);
                pSettingsDlg->keepOnTopOfParent();
                pSettingsDlg->setMainTB(&_toolBar);
                pSettingsDlg->open();
            }
            return true;
        }

        // GAME
        if (menuID == cMenuGame)
        {
            switch (actionID)
            {
                case cStartStopActionItem:
                    _mainView.startStop();
                    return true;

                case cResetActionItem:
                    _mainView.reset();
                    return true;

                default:
                    break;
            }
        }

        return false;
    }

    bool shouldClose() override
    {
        if (_mainView.isPlaying())
        {
            showAlert(tr("closeNOK"), tr("closeErr"));
            return false;
        }
        return true;
    }

public:
    MainWindow()
        : gui::Window(gui::Size(800, 800))
        , _imgStart(":start")
        , _imgStop(":stop")
        , _mainView(std::bind(&MainWindow::updateMenuAndTB, this))
        , _toolBar(&_mainView, &_imgStart)
    {
        setTitle(tr("appTitle"));
        _mainMenuBar.setAsMain(this);
        setToolBar(_toolBar);
        setCentralView(&_mainView);
    }
};
