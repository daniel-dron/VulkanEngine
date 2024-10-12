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

#include "camera.h"

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "engine/input.h"

void FirstPersonFlyingController::Update( float deltaTime ) {
    if ( EG_INPUT.IsKeyUp( EG_KEY::MOUSE_RIGHT ) ) {
        return;
    }

    m_moveSpeed += EG_INPUT.GetMouseWheel( ) * 10.0f;
    if ( m_moveSpeed <= 0.1f ) {
        m_moveSpeed = 0.1f;
    }

    glm::vec3 movement( 0.0f );

    if ( EG_INPUT.IsKeyDown( EG_KEY::W ) ) {
        movement += m_camera->GetFront( );
    }
    if ( EG_INPUT.IsKeyDown( EG_KEY::S ) ) {
        movement -= m_camera->GetFront( );
    }
    if ( EG_INPUT.IsKeyDown( EG_KEY::A ) ) {
        movement -= m_camera->GetRight( );
    }
    if ( EG_INPUT.IsKeyDown( EG_KEY::D ) ) {
        movement += m_camera->GetRight( );
    }

    // Normalize movement vector if it's not zero
    if ( glm::length2( movement ) > 0.0f ) {
        movement = glm::normalize( movement );
    }

    // Apply movement
    const glm::vec3 new_position = m_camera->GetPosition( ) + movement * m_moveSpeed * deltaTime;
    m_camera->SetPosition( new_position );

    auto [yaw, pitch] = EG_INPUT.GetMouseRel( );
    const float delta_yaw = static_cast<float>( yaw ) * m_sensitivity;
    const float delta_pitch = -static_cast<float>( pitch ) * m_sensitivity;
    m_camera->Rotate( delta_yaw, delta_pitch, 0.0f );
}

void FirstPersonFlyingController::DrawDebug( ) {
    ImGui::DragFloat( "Sensitivity", &m_sensitivity, 0.01f );
    ImGui::DragFloat( "Move Speed", &m_moveSpeed, 0.01f );
}

Camera::Camera( const Vec3 &position, float yaw, float pitch, float width, float height ) :
    m_position( position ), m_yaw( yaw ), m_pitch( pitch ) {
    SetAspectRatio( width, height );
    UpdateVectors( );
    UpdateMatrices( );
}

void Camera::SetAspectRatio( float width, float height ) {
    m_aspectRatio = width / height;

    m_dirtyMatrices = true;
}

void Camera::Rotate( float deltaYaw, float deltaPitch, float deltaRoll ) {
    m_yaw += deltaYaw;
    m_yaw = std::fmod( m_yaw, 360.0f );

    m_roll += deltaRoll;

    m_pitch += deltaPitch;
    m_pitch = std::clamp( m_pitch, m_minPitch, m_maxPitch );

    m_dirtyMatrices = true;

    UpdateVectors( );
}

const Vec3 &Camera::GetFront( ) const {
    return m_front;
}

const Vec3 &Camera::GetRight( ) const {
    return m_right;
}

const Vec3 &Camera::GetPosition( ) const {
    return m_position;
}

void Camera::SetPosition( const Vec3 &newPosition ) {
    m_position = newPosition;
}

const Mat4 &Camera::GetViewMatrix( ) {
    if ( m_dirtyMatrices ) {
        UpdateMatrices( );
    }

    return m_viewMatrix;
}

const Mat4 &Camera::GetProjectionMatrix( ) {
    if ( m_dirtyMatrices ) {
        UpdateMatrices( );
    }

    return m_projectionMatrix;
}

void Camera::UpdateVectors( ) {
    m_front.x = cos( glm::radians( m_yaw ) ) * cos( glm::radians( m_pitch ) );
    m_front.y = sin( glm::radians( m_pitch ) );
    m_front.z = sin( glm::radians( m_yaw ) ) * cos( glm::radians( m_pitch ) );
    m_front = normalize( m_front );

    m_right = normalize( cross( m_front, m_worldUp ) );
    m_up = normalize( cross( m_right, m_front ) );

    if ( m_roll != 0.0f ) {
        const glm::mat4 roll_matrix = rotate( glm::mat4( 1.0f ), glm::radians( m_roll ), m_front );
        m_right = glm::vec3( roll_matrix * glm::vec4( m_right, 0.0f ) );
        m_up = glm::vec3( roll_matrix * glm::vec4( m_up, 0.0f ) );
    }

    m_dirtyMatrices = true;
}

void Camera::UpdateMatrices( ) {
    m_viewMatrix = lookAt( m_position, m_position + m_front, m_up );
    m_projectionMatrix = glm::perspective( glm::radians( m_fov ), m_aspectRatio, m_nearPlane, m_farPlane );
    m_projectionMatrix[1][1] *= -1;

    m_dirtyMatrices = false;
}

void Camera::DrawDebug( ) {
    bool value_changed = false;

    ImGui::Indent( );
    const ImGuiTreeNodeFlags child_flags = ImGuiTreeNodeFlags_DefaultOpen;
    ImGui::GetStyle( ).IndentSpacing = 10.0f;

    // Position
    if ( ImGui::CollapsingHeader( "Position", child_flags ) ) {
        value_changed |= ImGui::InputFloat3( "Position", glm::value_ptr( m_position ) );
    }

    // Orientation vectors
    if ( ImGui::CollapsingHeader( "Orientation Vectors", child_flags ) ) {
        value_changed |= ImGui::InputFloat3( "Front", glm::value_ptr( m_front ) );
        value_changed |= ImGui::InputFloat3( "Right", glm::value_ptr( m_right ) );
        value_changed |= ImGui::InputFloat3( "Up", glm::value_ptr( m_up ) );
        value_changed |= ImGui::InputFloat3( "World Up", glm::value_ptr( m_worldUp ) );
    }

    // Rotation angles
    if ( ImGui::CollapsingHeader( "Rotation Angles", child_flags ) ) {
        value_changed |= ImGui::SliderFloat( "Yaw", &m_yaw, 0.0f, 360.0f );
        value_changed |= ImGui::SliderFloat( "Pitch", &m_pitch, m_minPitch, m_maxPitch );
        value_changed |= ImGui::SliderFloat( "Roll", &m_roll, -180.0f, 180.0f );
    }

    // FOV
    if ( ImGui::CollapsingHeader( "Field of View", child_flags ) ) {
        value_changed |= ImGui::SliderFloat( "FOV", &m_fov, m_minFov, m_maxFov );
        if ( ImGui::InputFloat( "Min FOV", &m_minFov ) || ImGui::InputFloat( "Max FOV", &m_maxFov ) ) {
            m_fov = glm::clamp( m_fov, m_minFov, m_maxFov );
            value_changed = true;
        }
    }

    // Other parameters
    if ( ImGui::CollapsingHeader( "Other Parameters", child_flags ) ) {
        value_changed |= ImGui::InputFloat( "Aspect Ratio", &m_aspectRatio );
        value_changed |= ImGui::InputFloat( "Near Plane", &m_nearPlane, 0.001f, 0.1f );
        value_changed |= ImGui::InputFloat( "Far Plane", &m_farPlane, 1.0f, 100.0f );
    }

    // Matrices
    if ( ImGui::CollapsingHeader( "Matrices", child_flags ) ) {
        ImGui::Text( "View Matrix" );
        for ( int i = 0; i < 4; i++ ) {
            ImGui::InputFloat4( ( "##View" + std::to_string( i ) ).c_str( ), glm::value_ptr( m_viewMatrix[i] ) );
        }

        ImGui::Text( "Projection Matrix" );
        for ( int i = 0; i < 4; i++ ) {
            ImGui::InputFloat4( ( "##Proj" + std::to_string( i ) ).c_str( ), glm::value_ptr( m_projectionMatrix[i] ) );
        }
    }

    // Set dirty flag if any relevant value changed
    if ( value_changed ) {
        UpdateVectors( );
    }

    // Dirty flag display
    ImGui::Checkbox( "Dirty Matrices", &m_dirtyMatrices );

    ImGui::Unindent( );
}
