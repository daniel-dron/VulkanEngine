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

#include "transform.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>
#include <glm/gtx/euler_angles.hpp>

Mat4 Transform::AsMatrix( ) const {
    const Mat4 trans = glm::translate( Mat4( 1.0f ), position );
    const Mat4 rot = glm::eulerAngleXYZ( euler.x, euler.y, euler.z );
    const Mat4 s = glm::scale( Mat4( 1.0f ), scale );
	return trans * rot * s;
}

void Transform::DrawDebug( const std::string& label ) {
	ImGui::PushID( label.c_str( ) );

	ImGui::Text( "%s", label.c_str( ) );
	ImGui::DragFloat3( "Position", &position[0], 0.1f );
	ImGui::DragFloat3( "Rotation", &euler[0], 0.1f );
	ImGui::DragFloat3( "Scale", &scale[0], 0.1f );

	ImGui::PopID( );
}