#pragma once

#include <gui/View.h>
#include <gui/Label.h>
#include <gui/ComboBox.h>
#include <gui/CheckBox.h>
#include <gui/LineEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/ToolBar.h>
#include <gui/Application.h>
#include <mu/IAppProperties.h>
#include <td/String.h>

class ViewSettings : public gui::View
{
protected:
    gui::Label _lblLangNow;
    gui::LineEdit _leLang;
    gui::Label _lblLangNew;
    gui::ComboBox _cmbLangs;
    gui::CheckBox _chbToolbarIconsAndLabels;
    gui::GridLayout _mainLayout;
    gui::ToolBar* _pMainTB = nullptr;

    int _initialLangSelection = 0;

public:
    ViewSettings(const gui::Size& /*sweeperSize*/, const gui::StatusBar* /*pSB*/, bool /*bAnimateTransition*/)
        : _lblLangNow(tr("lblLang"))
        , _lblLangNew(tr("lblLang2"))
        , _chbToolbarIconsAndLabels(tr("chbTBIcsAndLbls"))
        , _mainLayout(3, 2)
    {
        auto appProperties = getAppProperties();
        assert(appProperties);

        _leLang.setAsReadOnly();

        auto& langs = getSupportedLanguages();
        auto currTranslationIndex = getTranslationLanguageIndex();

        auto& strCurrentLanguage = langs[currTranslationIndex].getDescription();
        _leLang.setText(strCurrentLanguage);

        td::String strTr = appProperties->getValue("translation", "EN");

        int i = 0;
        for (const auto& lang : langs)
        {
            if (lang.getExtension() == strTr)
                _initialLangSelection = i;

            _cmbLangs.addItem(lang.getDescription());
            ++i;
        }

        auto maxCmbWidth = _cmbLangs.getWidthToFitLongestItem();
        _cmbLangs.setSizeLimits((td::UINT2)maxCmbWidth, gui::Control::Limit::Fixed);
        _leLang.setSizeLimits((td::UINT2)maxCmbWidth, gui::Control::Limit::Fixed);

        bool showLabels = appProperties->getTBLabelVisibility(mu::IAppProperties::ToolBarType::Main, true);
        _chbToolbarIconsAndLabels.setChecked(showLabels);

        _cmbLangs.selectIndex(_initialLangSelection);

        gui::GridComposer gc(_mainLayout);
        gc.appendRow(_lblLangNow) << _leLang;
        gc.appendRow(_lblLangNew) << _cmbLangs;
        gc.appendRow(_chbToolbarIconsAndLabels, 0);

        setLayout(&_mainLayout);

        _chbToolbarIconsAndLabels.onClick([this]()
        {
            if (_pMainTB)
                _pMainTB->showLabels(_chbToolbarIconsAndLabels.isChecked());
        });
    }

    td::String getTranslationExt()
    {
        td::String strExt;
        int currSelection = _cmbLangs.getSelectedIndex();

        if (currSelection >= 0)
        {
            auto& langs = getSupportedLanguages();
            strExt = langs[currSelection].getExtension();
        }
        return strExt;
    }

    void setMainTB(gui::ToolBar* pTB) { _pMainTB = pTB; }

    bool isRestartRequired() const
    {
        auto selectedLanguageIndex = _cmbLangs.getSelectedIndex();
        return (_initialLangSelection != selectedLanguageIndex);
    }
};
