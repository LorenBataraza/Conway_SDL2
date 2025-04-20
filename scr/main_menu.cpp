#include "imgui.h"
#include "appState.h"
//-----------------------------------------------------------------------------
// [SECTION] Example App: Main Menu Bar / ShowExampleAppMainMenuBar()
//-----------------------------------------------------------------------------
// - ShowExampleAppMainMenuBar()
// - ShowExampleMenuFile()
//-----------------------------------------------------------------------------

// Demonstrate creating a "main" fullscreen menu bar and populating it.
// Note the difference between BeginMainMenuBar() and BeginMenuBar():
// - BeginMenuBar() = menu-bar inside current window (which needs the ImGuiWindowFlags_MenuBar flag!)
// - BeginMainMenuBar() = helper to create menu-bar-sized window at the top of the main viewport + call BeginMenuBar() into it.

static void ShowExampleAppMainMenuBar(AppState* app_state){
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::MenuItem("Estructuras", NULL, &app_state->showStructures);
        ImGui::SameLine();
        ImGui::MenuItem("Parámetros", NULL, &app_state->showParameters);
        ImGui::SameLine();
        ImGui::MenuItem("Multiplayer", NULL, &app_state->showMultiplayerConf);

        ImGui::EndMainMenuBar();
    }
}

