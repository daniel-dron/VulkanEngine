#include <pch.h>

#include "transform.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>
#include <glm/gtx/euler_angles.hpp>

mat4 Transform::asMatrix( ) const {
	mat4 trans = glm::translate( mat4( 1.0f ), position );
	mat4 rot = glm::eulerAngleXYZ( euler.x, euler.y, euler.z );
	mat4 s = glm::scale( mat4( 1.0f ), scale );
	return trans * rot * s;
}

void Transform::drawDebug( const std::string& label ) {
	ImGui::PushID( label.c_str( ) );

	ImGui::Text( "%s", label.c_str( ) );
	ImGui::DragFloat3( "Position", &position[0], 0.1f );
	ImGui::DragFloat3( "Rotation", &euler[0], 0.1f );
	ImGui::DragFloat3( "Scale", &scale[0], 0.1f );

	ImGui::PopID( );
}

void Transform::DrawGizmo( ) {
	//auto camera_view = scene_data.view;
	//auto camera_proj = scene_data.proj;
	//camera_proj[1][1] *= -1;

	//auto& point_light = point_lights.at( 0 );
	//mat4 tc = point_light.transform.asMatrix( );
	//if ( ImGuizmo::Manipulate( glm::value_ptr( camera_view ), glm::value_ptr( camera_proj ), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD, glm::value_ptr( tc ) ) ) {
	//	vec3 skew{};
	//	vec4 perspective{};
	//	quat rotation{};
	//	vec3 scale{};
	//	vec3 position{};

	//	glm::decompose( tc, scale, rotation, position, skew, perspective );
	//	glm::extractEulerAngleXYZ( glm::mat4_cast( rotation ), rotation.x, rotation.y, rotation.z );
	//	point_light.transform.position = position;
	//	point_light.transform.model = tc;

	//}

}
