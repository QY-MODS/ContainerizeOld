#pragma once
#include "Manager.h"
#include "SKSEMenuFramework.h"

namespace UI {

    std::string last_generated = "";
    Manager* M = nullptr;
    std::vector<Source> sources;
    std::map<FormID, std::vector<std::string>> locations;
    int item_current_idx = 0;

    ImGuiTableFlags table_flags =
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;


    void __stdcall RenderSettings();
    void __stdcall RenderInspect();

    void Register(Manager* manager) {
        SKSEMenuFramework::SetSection(Utilities::mod_name);
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
        SKSEMenuFramework::AddSectionItem("Inspect", RenderInspect);
        M = manager;
    }

    
    void __stdcall RenderSettings() {
        if (ImGui::BeginTable("table_settings", 2, table_flags)) {
            for (const auto& [setting_name, setting] : M->_other_settings) {
                // we just want to display the settings in read only mode
                const char* value = setting ? "Enabled" : "Disabled";
                const auto color = setting ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(setting_name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(color, value);
            }
            ImGui::EndTable();
        }
    };

    void __stdcall RenderInspect() {
        if (!M) {
            ImGui::Text("Not available");
            return;
        }

        FontAwesome::PushSolid();
        if (ImGui::Button((FontAwesome::UnicodeToUtf8(0xf021) + " Refresh").c_str())) {
            last_generated = std::format("{} (in-game hours)", RE::Calendar::GetSingleton()->GetHoursPassed());
            sources = M->GetSources();
            locations.clear();
            item_current_idx = 0;

            for (const auto& source : sources) {
                if (!source.formid) continue;
                std::vector<std::string> temp_locations;
				for (const auto& item : source.data) {
                    RE::TESForm* temp_form;
                    const auto id = item.second;
                    const auto chest_id = item.first;
                    if (id == chest_id) {
                        temp_form = RE::PlayerCharacter::GetSingleton()->As<RE::TESForm>();
                    }
                    else if (M->IsChest(id)) {
                        temp_form = RE::TESForm::LookupByID(M->GetRealFakePairOfChest(id).second);
                    } else {
                        const auto temp_location = RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
                        temp_form = temp_location ? temp_location->GetBaseObject() : nullptr;
                    }
                    const char* location_name = temp_form ? temp_form->GetName() : std::to_string(id).c_str();
                    temp_locations.push_back(std::string(location_name));
				}
				locations[source.formid] = temp_locations;
			}
        }
        FontAwesome::Pop();
        ImGui::SameLine();
        ImGui::Text(("Last Generated: " + last_generated).c_str());

        ImGui::Text("Containers");
        if (!sources.empty() && ImGui::BeginTable("table_inspect", 3, table_flags)) {
            ImGui::TableSetupColumn("Container");
            ImGui::TableSetupColumn("Weight Limit");
            ImGui::TableSetupColumn("Count");
            ImGui::TableHeadersRow();
            for (const auto& source : sources) {
                // we just want to display the settings in read only mode
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(std::string(source.GetName()).c_str());
                ImGui::TableNextColumn();
                ImGui::Text(std::to_string(source.capacity).c_str());
                ImGui::TableNextColumn();
                ImGui::Text(std::to_string(source.data.size()).c_str());
                //bool hovered = ImGui::IsItemHovered();
                //ImVec2(-FLT_MIN, (hovered ? 5 : 1) * ImGui::GetTextLineHeightWithSpacing()))
                //if (ImGui::BeginListBox("listbox_locations")) {
                //    const auto items = locations[source.formid];
                //    for (int n = 0; n < items.size(); n++) {
                //        const bool is_selected = (item_current_idx == n);
                //        if (ImGui::Selectable(items[n].c_str(), is_selected)) item_current_idx = n;

                //        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                //        if (is_selected) ImGui::SetItemDefaultFocus();
                //    }
                //    ImGui::EndListBox();
                //}
            }
            ImGui::EndTable();
        }

    }

}