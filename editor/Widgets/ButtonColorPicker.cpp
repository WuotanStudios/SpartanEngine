/*
Copyright(c) 2016-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ========================
#include "ButtonColorPicker.h"
#include "../ImGui/Source/imgui.h"
#include "../ImGui/ImGui_Extension.h"
//===================================

//= NAMESPACES =====
using namespace std;
//==================

// Names and order is derived from Color.h
static vector<string> color_names
{
    "aluminum",
    "blood",
    "bone",
    "brass",
    "brick",
    "charcoal",
    "chocolate",
    "chromium",
    "cobalt",
    "concrete",
    "cooking_oil",
    "copper",
    "diamond",
    "egg_shell",
    "eye_cornea",
    "eye_lens",
    "eye_sclera",
    "glass",
    "gold",
    "gray_card",
    "honey",
    "ice",
    "iron",
    "ketchup",
    "lead",
    "mercury",
    "milk",
    "nickel",
    "office_paper",
    "plastic_pc",
    "plastic_pet",
    "plastic_acrylic",
    "plastic_pp",
    "plastic_pvc",
    "platinum",
    "salt",
    "sand",
    "sapphire",
    "silver",
    "skin_1",
    "skin_2",
    "skin_3",
    "skin_4",
    "skin_5",
    "skin_6",
    "snow",
    "tire",
    "titanium",
    "tungsten",
    "vanadium",
    "water",
    "zinc"
};

static vector<Spartan::Color> color_values
{
     Spartan::Color(0.912f, 0.914f, 0.920f),
     Spartan::Color(0.644f, 0.003f, 0.005f),
     Spartan::Color(0.793f, 0.793f, 0.664f),
     Spartan::Color(0.887f, 0.789f, 0.434f),
     Spartan::Color(0.262f, 0.095f, 0.061f),
     Spartan::Color(0.020f, 0.020f, 0.020f),
     Spartan::Color(0.162f, 0.091f, 0.060f),
     Spartan::Color(0.550f, 0.556f, 0.554f),
     Spartan::Color(0.662f, 0.655f, 0.634f),
     Spartan::Color(0.510f, 0.510f, 0.510f),
     Spartan::Color(0.738f, 0.687f, 0.091f),
     Spartan::Color(0.926f, 0.721f, 0.504f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.610f, 0.624f, 0.631f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.680f, 0.490f, 0.370f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.944f, 0.776f, 0.373f),
     Spartan::Color(0.180f, 0.180f, 0.180f),
     Spartan::Color(0.831f, 0.397f, 0.038f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.531f, 0.512f, 0.496f),
     Spartan::Color(0.164f, 0.006f, 0.002f),
     Spartan::Color(0.632f, 0.626f, 0.641f),
     Spartan::Color(0.781f, 0.779f, 0.779f),
     Spartan::Color(0.604f, 0.584f, 0.497f),
     Spartan::Color(0.649f, 0.610f, 0.541f),
     Spartan::Color(0.738f, 0.768f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.679f, 0.642f, 0.588f),
     Spartan::Color(0.800f, 0.800f, 0.800f),
     Spartan::Color(0.440f, 0.386f, 0.231f),
     Spartan::Color(0.670f, 0.764f, 0.855f),
     Spartan::Color(0.962f, 0.949f, 0.922f),
     Spartan::Color(0.847f, 0.638f, 0.552f),
     Spartan::Color(0.799f, 0.485f, 0.347f),
     Spartan::Color(0.600f, 0.310f, 0.220f),
     Spartan::Color(0.430f, 0.200f, 0.130f),
     Spartan::Color(0.360f, 0.160f, 0.080f),
     Spartan::Color(0.090f, 0.050f, 0.020f),
     Spartan::Color(0.810f, 0.810f, 0.810f),
     Spartan::Color(0.023f, 0.023f, 0.023f),
     Spartan::Color(0.616f, 0.582f, 0.544f),
     Spartan::Color(0.925f, 0.835f, 0.757f),
     Spartan::Color(0.945f, 0.894f, 0.780f),
     Spartan::Color(1.000f, 1.000f, 1.000f),
     Spartan::Color(0.875f, 0.867f, 0.855f)
};

ButtonColorPicker::ButtonColorPicker(const string& window_title)
{
    m_window_title       = window_title;
    m_color_picker_label = "##" + window_title + "_1";
}

void ButtonColorPicker::Update()
{
    // button
    if (ImGui::ColorButton("##color_button", ImVec4(m_color.r, m_color.g, m_color.b, m_color.a)))
    {
        m_is_visible = true;
    }

    // picker
    if (m_is_visible)
    {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_window_title.c_str(), &m_is_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGuiColorEditFlags       flags = ImGuiColorEditFlags_AlphaBar;
                                  flags |= m_show_wheel ? ImGuiColorEditFlags_PickerHueWheel : ImGuiColorEditFlags_PickerHueBar;
        if (!m_show_preview)      flags |= ImGuiColorEditFlags_NoSidePreview;
        if (m_show_rgb)           flags |= ImGuiColorEditFlags_DisplayRGB;
        if (m_show_hsv)           flags |= ImGuiColorEditFlags_DisplayHSV;
        if (m_show_hex)           flags |= ImGuiColorEditFlags_DisplayHex;
        if (m_hdr)                flags |= ImGuiColorEditFlags_HDR;
        if (m_alpha_half_preview) flags |= ImGuiColorEditFlags_AlphaPreviewHalf;
        if (m_alpha_preview)      flags |= ImGuiColorEditFlags_AlphaPreview;
        if (!m_options_menu)      flags |= ImGuiColorEditFlags_NoOptions;

        ImGui::ColorPicker4(m_color_picker_label.c_str(), (float*)&m_color, flags);

        ImGui::Separator();

        // Note: Using hardcoded labels so the settings remain the same for all color pickers

        // WHEEL
        ImGui::Text("Wheel");
        ImGui::SameLine(); ImGui::Checkbox("##ButtonColorPickerWheel", &m_show_wheel);

        // RGB
        ImGui::SameLine(); ImGui::Text("RGB");
        ImGui::SameLine(); ImGui::Checkbox("##ButtonColorPickerRGB", &m_show_rgb);

        // HSV
        ImGui::SameLine(); ImGui::Text("HSV");
        ImGui::SameLine(); ImGui::Checkbox("##ButtonColorPickerHSV", &m_show_hsv);

        // HEX
        ImGui::SameLine(); ImGui::Text("HEX");
        ImGui::SameLine(); ImGui::Checkbox("##ButtonColorPickerHEX", &m_show_hex);

        // Default colors combo box
        {
            ImGui::Text("Physically based colors");
            ImGui::SameLine();
            if (ImGuiSp::combo_box("##physically_based_colors", color_names, &m_combo_box_index))
            {
                m_color = color_values[m_combo_box_index];
            }
        }

        ImGui::End();
    }
}
