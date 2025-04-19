#include "imgui.h"
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
static void ShowExampleAppMainMenuBar(bool *run_sim , int* frecuencia)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Parametros"))
        {
	        ImGui::Checkbox("Run", run_sim);
    
            static int n = 0;
	        ImGui::DragInt("Hz", frecuencia, 1, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
            
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

