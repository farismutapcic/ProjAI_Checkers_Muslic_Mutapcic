#pragma once
#include <gui/Dialog.h>
#include <gui/Application.h>
#include <gui/Alert.h>
#include "ViewSettings.h"
#include <cassert>

class DialogSettings : public gui::Dialog
{
protected:
    ViewSettings _viewSettings;

    bool onClick(Dialog::Button::ID btnID, gui::Button* pButton) override
    {
        if (btnID == Dialog::Button::ID::OK)
        {
            auto appProperties = getAppProperties();
            assert(appProperties);

            td::String strTr = _viewSettings.getTranslationExt();
            if (strTr.length() > 0)
                appProperties->setValue("translation", strTr);

            if (_viewSettings.isRestartRequired())
            {
                gui::Alert::showYesNoQuestion(
                    tr("RestartRequired"),
                    tr("RestartRequiredInfo"),
                    tr("Restart"),
                    tr("DoNoRestart"),
                    [this](gui::Alert::Answer answer)
                    {
                        if (answer == gui::Alert::Answer::Yes)
                            if (auto pApp = getApplication()) pApp->restart();
                    }
                );
            }
        }
        return true;
    }

public:
    DialogSettings(gui::Frame* pFrame, td::UINT4 wndID = 0)
        : gui::Dialog(
            pFrame,
            { {gui::Dialog::Button::ID::OK, tr("Ok"), gui::Button::Type::Default},
              {gui::Dialog::Button::ID::Cancel, tr("Cancel")} },
            gui::Size(450, 150),
            wndID)
        , _viewSettings(gui::Size(600, 400), nullptr, false)
    {
        setTitle(tr("dlgSettings"));
        setCentralView(&_viewSettings);
    }

    void setMainTB(gui::ToolBar* pTB) { _viewSettings.setMainTB(pTB); }
};
