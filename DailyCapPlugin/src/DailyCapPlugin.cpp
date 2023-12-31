#include "DailyCapPlugin.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

import PluginUtils;

namespace {
    std::string getAccountEmail() {
        auto context = GW::GetCharContext();
        if (!context) {
            return "null@arena.net";
        }

        return PluginUtils::WStringToString(context->player_email);
    }
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static DailyCapPlugin instance;
    return &instance;
}

DailyCapPlugin::DailyCapPlugin() :
    gladiator_cap(DailyCapWithBonusWeek("Gladiator", 200, 0)),
    champion_cap(DailyCapWithBonusWeek("Champion", 10, 1)),
    hero_cap(DailyCapWithBonusWeek("Fame", 300, 3)),
    codex_cap(DailyCapWithBonusWeek("Codex", 100, 4)),
    gladiator_box_cap(DailyCap("Gladiator's Strongbox", 5, true)),
    champion_box_cap(DailyCap("Champion's Strongbox", 5, true)),
    hero_box_cap(DailyCap("Hero's Strongbox", 5, true)),
    codex_box_cap(DailyCap("Strategist's Strongbox", 5, true)),
    zkey_cap(DailyCap("Zaishen Key", 25, true)),
    tracked_caps(make_tracked_caps()), hook_entry({}) {
}

std::vector<DailyCap*> DailyCapPlugin::make_tracked_caps() {
    auto result = std::vector<DailyCap*>();

    result.push_back(&this->gladiator_cap);
    result.push_back(&this->champion_cap);
    result.push_back(&this->hero_cap);
    result.push_back(&this->codex_cap);

    result.push_back(&this->gladiator_box_cap);
    result.push_back(&this->champion_box_cap);
    result.push_back(&this->hero_box_cap);
    result.push_back(&this->codex_box_cap);

    result.push_back(&this->zkey_cap);

    return result;
}

void DailyCapPlugin::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE hmodule) {
    ToolboxUIPlugin::Initialize(ctx, fns, hmodule);

    const auto fwd_OnUpdateTitle = [](GW::HookStatus*, GW::Packet::StoC::UpdateTitle* packet) {
        auto instance = reinterpret_cast<DailyCapPlugin*>(ToolboxPluginInstance());
        instance->OnTitleUpdated(
                static_cast<GW::Constants::TitleID>(packet->title_id),
                static_cast<int>(packet->new_value)
        );
    };
    const auto fwd_OnTitleInfo = [](GW::HookStatus*, GW::Packet::StoC::TitleInfo* packet) {
        auto instance = reinterpret_cast<DailyCapPlugin*>(ToolboxPluginInstance());
        instance->OnTitleUpdated(
                static_cast<GW::Constants::TitleID>(packet->title_id),
                static_cast<int>(packet->value)
        );
    };
    const auto fwd_OnServerChatMessage = [](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
        auto instance = reinterpret_cast<DailyCapPlugin*>(ToolboxPluginInstance());
        auto* w = GW::GetWorldContext();
        if(w && w->message_buff.valid()) {
            instance->OnServerChatMessage(w->message_buff.begin());
        }
    };
    const auto fwd_OnSendDialog = [](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        auto instance = reinterpret_cast<DailyCapPlugin*>(ToolboxPluginInstance());
        instance->OnDialogSent(reinterpret_cast<uint32_t>(wparam));
    };

    // Both UpdateTitle and TitleInfo are required, because TitleInfo is sent instead of UpdateTitle when a title ranks up
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::UpdateTitle>(&this->hook_entry, fwd_OnUpdateTitle, 1);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TitleInfo>(&this->hook_entry, fwd_OnTitleInfo, 1);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&this->hook_entry, fwd_OnServerChatMessage);
    GW::UI::RegisterUIMessageCallback(&this->hook_entry, GW::UI::UIMessage::kSendDialog, fwd_OnSendDialog);
}

void DailyCapPlugin::SignalTerminate() {
    GW::StoC::RemoveCallback<GW::Packet::StoC::UpdateTitle>(&this->hook_entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::TitleInfo>(&this->hook_entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&this->hook_entry);

    ToolboxUIPlugin::SignalTerminate();
}

void DailyCapPlugin::Update(float) {
    if (this->should_save && this->last_settings_folder) {
        this->should_save = false;

        this->SaveSettings(this->last_settings_folder);
    }
}

void DailyCapPlugin::Draw(IDirect3DDevice9*) {
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        auto default_text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, this->progress_bar_background_color);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, this->progress_bar_foreground_color);
        ImGui::PushStyleColor(ImGuiCol_Text, this->progress_bar_text_color);

        for (const auto& cap : tracked_caps) {
            ImGui::PushStyleColor(ImGuiCol_Text, default_text_color);
            this->should_save |= cap->DrawInternal();
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

void DailyCapPlugin::DrawSettings() {
    ToolboxUIPlugin::DrawSettings();

    ImGui::Separator();
    ImGui::Text("Display:");
    ImGui::Indent();

    constexpr float COLUMN_WIDTH = 200.0f;
    const float available_width = ImGui::GetContentRegionAvail().x;
    const int num_columns = static_cast<int>(std::floor(available_width / COLUMN_WIDTH));
    float indent = ImGui::GetCurrentWindow()->DC.Indent.x;
    int current_column = 0;

    for (const auto& cap : tracked_caps) {
        if (current_column > 0) {
            if (current_column < num_columns) {
                ImGui::SameLine(static_cast<float>(current_column) * COLUMN_WIDTH + indent);
            }
            else {
                current_column = 0;
            }
        }
        current_column++;
        cap->DrawSettingsInternal();
    }
    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("Colors:");
    ImGui::Indent();
    ImGui::ColorEdit3("Background", reinterpret_cast<float*>(&progress_bar_background_color), ImGuiColorEditFlags_NoAlpha);
    ImGui::ColorEdit3("Foreground", reinterpret_cast<float*>(&progress_bar_foreground_color), ImGuiColorEditFlags_NoAlpha);
    ImGui::ColorEdit3("Text", reinterpret_cast<float*>(&progress_bar_text_color), ImGuiColorEditFlags_NoAlpha);
    ImGui::Unindent();
}

void DailyCapPlugin::LoadSettings(const wchar_t* folder) {
    ToolboxUIPlugin::LoadSettings(folder);

    constexpr int DEFAULT_PROGRESS_BAR_BG_COLOR = -14071174; // ARGB 0xFF294A7A
    constexpr int DEFAULT_PROGRESS_BAR_FG_COLOR = -9730641;  // ARGB 0xFF6B85AF
    constexpr int DEFAULT_PROGRESS_BAR_TEXT_COLOR = -1;      // ARGB 0xFFFFFFFF

    auto progress_bg_color = static_cast<uint32_t>(ini.GetLongValue(this->Name(), "progress_bar_background_color", DEFAULT_PROGRESS_BAR_BG_COLOR));
    this->progress_bar_background_color = ImGui::ColorConvertU32ToFloat4(progress_bg_color);

    auto progress_fg_color = static_cast<uint32_t>(ini.GetLongValue(this->Name(), "progress_bar_foreground_color", DEFAULT_PROGRESS_BAR_FG_COLOR));
    this->progress_bar_foreground_color = ImGui::ColorConvertU32ToFloat4(progress_fg_color);

    auto progress_text_color = static_cast<uint32_t>(ini.GetLongValue(this->Name(), "progress_bar_text_color", DEFAULT_PROGRESS_BAR_TEXT_COLOR));
    this->progress_bar_text_color = ImGui::ColorConvertU32ToFloat4(progress_text_color);

    for (auto& cap: tracked_caps) {
        cap->LoadSettings(ini, this->Name());
    }

    std::string account_email = getAccountEmail();

    auto gladiator = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Gladiator);
    auto champion = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Champion);
    auto hero = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Hero);
    auto codex = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Codex);
    gladiator_cap.LoadProgress(ini, account_email.c_str(),
                               gladiator ? static_cast<int>(gladiator->current_points) : 0);
    champion_cap.LoadProgress(ini, account_email.c_str(),
                              champion ? static_cast<int>(champion->current_points) : 0);
    hero_cap.LoadProgress(ini, account_email.c_str(), hero ?
                                                      static_cast<int>(hero->current_points) : 0);
    codex_cap.LoadProgress(ini, account_email.c_str(),
                           codex ? static_cast<int>(codex->current_points) : 0);

    gladiator_box_cap.LoadProgress(ini, account_email.c_str(), 0);
    champion_box_cap.LoadProgress(ini, account_email.c_str(), 0);
    hero_box_cap.LoadProgress(ini, account_email.c_str(), 0);
    codex_box_cap.LoadProgress(ini, account_email.c_str(), 0);

    zkey_cap.LoadProgress(ini, account_email.c_str(), 0);
}

void DailyCapPlugin::SaveSettings(const wchar_t* folder) {
    std::string account_email = getAccountEmail();

    auto progress_bg_color = static_cast<int>(ImGui::ColorConvertFloat4ToU32(this->progress_bar_background_color));
    ini.SetLongValue(this->Name(), "progress_bar_background_color", progress_bg_color);

    auto progress_fg_color = static_cast<int>(ImGui::ColorConvertFloat4ToU32(this->progress_bar_foreground_color));
    ini.SetLongValue(this->Name(), "progress_bar_foreground_color", progress_fg_color);

    auto progress_text_color = static_cast<int>(ImGui::ColorConvertFloat4ToU32(this->progress_bar_text_color));
    ini.SetLongValue(this->Name(), "progress_bar_text_color", progress_text_color);

    for (const auto& cap : tracked_caps) {
        cap->SaveSettings(ini, this->Name());
        cap->SaveProgress(ini, account_email.c_str());
    }

    ToolboxUIPlugin::SaveSettings(folder);
}

void DailyCapPlugin::OnTitleUpdated(GW::Constants::TitleID title_id, int new_value) {
    using enum GW::Constants::TitleID;

    switch (title_id) {
        case Gladiator:
            this->should_save |= this->tracked_caps[0]->SetValue(new_value);
            break;
        case Champion:
            this->should_save |= this->tracked_caps[1]->SetValue(new_value);
            break;
        case Hero:
            this->should_save |= this->tracked_caps[2]->SetValue(new_value);
            break;
        case Codex:
            this->should_save |= this->tracked_caps[3]->SetValue(new_value);
            break;
        default:
            break;
    }
}

void DailyCapPlugin::OnServerChatMessage(const wchar_t* message_enc) {
    constexpr std::wstring_view CHAMPION_STRONGBOX_MESSAGE = L"\x8101\x0a4c\xdaa1\x8703\x5065\x010a\x8102\x7529\xfe60\xc44b\x25d3\x0001\x0000";
    constexpr std::wstring_view CODEX_STRONGBOX_MESSAGE = L"\x8101\x0a4c\xdaa1\x8703\x5065\x010a\x8102\x752f\xddd3\x9391\x50b4\x0001\x0000";
    constexpr std::wstring_view GLADIATOR_STRONGBOX_MESSAGE = L"\x8101\x0a4c\xdaa1\x8703\x5065\x010a\x8102\x752d\xc6ff\xd91c\x258b\x0001\x0000";
    constexpr std::wstring_view HERO_STRONGBOX_MESSAGE = L"\x8101\x0a4c\xdaa1\x8703\x5065\x010a\x8102\x752b\xdc27\xe8ad\x7d70\x0001\x0000";
    constexpr std::wstring_view ZKEY_MESSAGE = L"\x2afd\xb4fb\xd8c8\x1f00\x010a\x8101\x6b83\x0001\x010b\x8102\x36f4\x0001\x0000";

    if (!message_enc) {
        return;
    }

    // NB. we always want to save to persist the config whenever one of these is updated
    // as we can't infer their values from packets etc should the client crash before
    // their progress is persisted.
    if (this->expecting_zkey_message && 0 == wcscmp(message_enc, ZKEY_MESSAGE.data())) {
        this->zkey_cap.AddValue(1);
        this->should_save = true;
        this->expecting_zkey_message = false;
    }
    if (0 == wcscmp(message_enc, GLADIATOR_STRONGBOX_MESSAGE.data())) {
        this->gladiator_box_cap.AddValue(1);
        this->should_save = true;
    }
    else if (0 == wcscmp(message_enc, CHAMPION_STRONGBOX_MESSAGE.data())) {
        this->champion_box_cap.AddValue(1);
        this->should_save = true;
    }
    else if (0 == wcscmp(message_enc, HERO_STRONGBOX_MESSAGE.data())) {
        this->hero_box_cap.AddValue(1);
        this->should_save = true;
    }
    else if (0 == wcscmp(message_enc, CODEX_STRONGBOX_MESSAGE.data())) {
        this->codex_box_cap.AddValue(1);
        this->should_save = true;
    }
}

void DailyCapPlugin::OnDialogSent(uint32_t dialog_id) {
    // TODO: double-check whether buying zkey with reward points uses the same dialog id
    constexpr int TOLKANO_ZKEY_DIALOG_ID = 0x88;

    this->expecting_zkey_message = (dialog_id == TOLKANO_ZKEY_DIALOG_ID);
}