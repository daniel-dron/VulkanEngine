#include "light.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>    

void PointLight::DrawDebug( ) {
	ImGui::ColorEdit3( "Color HSV", &hsv.hue, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputHSV | ImGuiColorEditFlags_PickerHueWheel );
	ImGui::DragFloat( "Power", &power, 0.1f, 0.0f );
	ImGui::DragFloat( "Constant", &constant, 0.01f, 0.0f, 1.0f );
	ImGui::DragFloat( "Linear", &linear, 0.01f, 0.0f, 1.0f );
	ImGui::DragFloat( "Quadratic", &quadratic, 0.01f, 0.0f, 1.0f );
}