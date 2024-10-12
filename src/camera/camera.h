#pragma once

#include <vk_types.h>

class Camera {
public:
    Camera( const Vec3 &position, float yaw, float pitch, float width, float height );

    void SetAspectRatio( float width, float height );
    void Rotate( float deltaYaw, float deltaPitch, float deltaRoll = 0.0f );

    const Vec3 &GetFront( ) const;
    const Vec3 &GetRight( ) const;
    const Vec3 &GetPosition( ) const;
    void SetPosition( const Vec3 &newPosition );

    const Mat4 &GetViewMatrix( );
    const Mat4 &GetProjectionMatrix( );

    void DrawDebug( );

private:
    void UpdateVectors( );
    void UpdateMatrices( );

    Vec3 m_position = { 0.0f, 0.0f, 0.0f };
    Vec3 m_front = GLOBAL_FRONT;
    Vec3 m_right = GLOBAL_RIGHT;
    Vec3 m_up = GLOBAL_UP;
    Vec3 m_worldUp = GLOBAL_UP;

    Mat4 m_viewMatrix = glm::mat4( 1.0f );
    Mat4 m_projectionMatrix = glm::mat4( 1.0f );

    float m_yaw = 0.0f;
    float m_roll = 0.0f;
    float m_pitch = 0.0f;

    float m_minPitch = -89.0f;
    float m_maxPitch = 89.0f;

    float m_fov = 90.0f;
    float m_maxFov = 130.0f;
    float m_minFov = 20.0f;

    float m_aspectRatio = 0.0f;

    float m_nearPlane = 0.01f;
    float m_farPlane = 200.0f;

    bool m_dirtyMatrices = true;
};

class CameraController {
public:
    explicit CameraController( Camera *cam ) :
        m_camera( cam ) {}

    virtual void Update( float delta ) = 0;
    virtual void DrawDebug( ) = 0;

protected:
    Camera *m_camera;
};

class FirstPersonFlyingController : public CameraController {
public:
    explicit FirstPersonFlyingController( Camera *cam, float sens = 0.1f, float speed = 5.0f ) :
        CameraController( cam ), m_sensitivity( sens ), m_moveSpeed( speed ) {}

    void Update( float deltaTime ) override;
    void DrawDebug( ) override;

private:
    float m_sensitivity;
    float m_moveSpeed;
};
