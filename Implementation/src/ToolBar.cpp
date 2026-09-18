#include "ToolBar.h"

ToolBar::ToolBar(ViewCheckersBoard* /*pView*/, gui::Image* imgRun)
    : gui::ToolBar("mainTB", 3)
    , _imgReset(":reset")
    , _imgSettings(":settings")
{
    addItem(tr("settings"), &_imgSettings, tr("settingsTT"),
            cMenuApp, 0, 0, cSettingsActionItem);

    addItem(tr("reset"), &_imgReset, tr("resetTT"),
            cMenuGame, 0, 0, cResetActionItem);

    addItem(tr("start"), imgRun, tr("startTT"),
            cMenuGame, 0, 0, cStartStopActionItem);
}
