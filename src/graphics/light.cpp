/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#include <pch.h>

#include "light.h"
#include <imgui.h>

void PointLight::DrawDebug( ) {
	ImGui::ColorEdit3( "Color HSV", &hsv.hue, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputHSV | ImGuiColorEditFlags_PickerHueWheel );
	ImGui::DragFloat( "Power", &power, 0.1f, 0.0f );
	ImGui::DragFloat( "Constant", &constant, 0.01f, 0.0f, 1.0f );
	ImGui::DragFloat( "Linear", &linear, 0.01f, 0.0f, 1.0f );
	ImGui::DragFloat( "Quadratic", &quadratic, 0.01f, 0.0f, 1.0f );
}