#pragma once
#include <gui/MenuBar.h>
#include "Constants.h"

class MenuBar : public gui::MenuBar
{
private:
    gui::SubMenu _subFirst;
    gui::SubMenu _subGame;

protected:
    void populateFirstMenu()
    {
        auto& items = _subFirst.getItems();
        items[0].initAsActionItem(tr("settings"), cSettingsActionItem);
        items[1].initAsQuitAppActionItem(tr("Quit"), "q");
    }

    void populateGameMenu()
    {
        auto& items = _subGame.getItems();
        items[0].initAsActionItem(tr("start"), cStartStopActionItem);
        items[0].setAsCheckable();
    }

public:
    MenuBar()
        : gui::MenuBar(2)
        , _subFirst(cMenuApp, tr("App"), 2)
        , _subGame(cMenuGame, tr("Game"), 1)
    {
        populateFirstMenu();
        populateGameMenu();
        _menus[0] = &_subFirst;
        _menus[1] = &_subGame;
    }
};
