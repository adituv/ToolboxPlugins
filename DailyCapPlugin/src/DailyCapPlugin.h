#pragma once

#include <cstdint>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>
#include <ToolboxUIPlugin.h>

#include "DailyCap.h"
#include "DailyCapWithBonusWeek.h"

class DailyCapPlugin : public ToolboxUIPlugin {
public:
    [[nodiscard]] const char* Name() const override { return "Daily Cap Tracker"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_DOLLAR_SIGN; }
    [[nodiscard]] bool HasSettings() const override { return true; }

    DailyCapPlugin();

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

    void OnTitleUpdated(GW::Constants::TitleID title_id, int new_value);
    void OnServerChatMessage(const wchar_t* message_enc);
    void OnDialogSent(uint32_t dialog_id);
private:
    DailyCapWithBonusWeek gladiator_cap;
    DailyCapWithBonusWeek champion_cap;
    DailyCapWithBonusWeek hero_cap;
    DailyCapWithBonusWeek codex_cap;

    DailyCap gladiator_box_cap;
    DailyCap champion_box_cap;
    DailyCap hero_box_cap;
    DailyCap codex_box_cap;
    DailyCap zkey_cap;

    ImVec4 progress_bar_background_color;
    ImVec4 progress_bar_foreground_color;
    ImVec4 progress_bar_text_color;

    bool expecting_zkey_message = false;
    bool should_save = false;
    const wchar_t* last_settings_folder = nullptr;

    const std::vector<DailyCap*> tracked_caps;
    GW::HookEntry hook_entry;

    std::vector<DailyCap*> make_tracked_caps();
};