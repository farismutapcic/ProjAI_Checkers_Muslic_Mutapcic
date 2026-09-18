#pragma once
#include <gui/ToolBar.h>
#include <gui/Image.h>
#include "Constants.h"

class ViewCheckersBoard;

class ToolBar : public gui::ToolBar
{
    gui::Image _imgReset;
    gui::Image _imgSettings;

public:
    ToolBar(ViewCheckersBoard* pView, gui::Image* imgRun);
};
