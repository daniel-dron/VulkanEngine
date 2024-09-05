#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "bindless.glsl"
#include "vertex.glsl"

layout (buffer_reference, scalar) readonly buffer IblMatrices {
    mat4 proj;
    mat4 view[6];
} iblMatrices;

layout (push_constant) uniform constants {
    VertexBuffer vertex_buffer;
    IblMatrices iblMatrices;
    int skybox;
} pc;

layout (location = 0) in vec3 in_pos;

layout (location = 0) out vec4 out_color;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

const float PI = 3.14159265359;
void main( ) {
    vec3 N = normalize(in_pos);

    vec3 irradiance = vec3(0.0);   
    
    // tangent space calculation from origin point
    vec3 up    = vec3(0.0, -1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));
       
    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N; 

            vec2 uvs = SampleSphericalMap(sampleVec);
            vec3 color = sampleTexture2DLinear(pc.skybox, fract(uvs)).rgb;
            irradiance += color * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    out_color = vec4(irradiance, 1.0);
}