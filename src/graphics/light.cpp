#include "light.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

void PointLight::DrawDebug()
{
    ImGui::DragFloat3("Color", glm::value_ptr(color), 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Constant", &constant, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Linear", &linear, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Quadratic", &quadratic, 0.01f, 0.0f, 1.0f);
}