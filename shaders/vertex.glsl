struct Vertex {
    vec4 Position;   // 16 bytes (vec3 position + float uv_x)
    vec4 Normal;     // 16 bytes (vec3 normal + float uv_y)
    vec4 Tangent;    // 16 bytes (vec3 tangent + padding)
    vec4 Bitangent;  // 16 bytes (vec3 bitangent + padding)
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer VertexBuffer {
    Vertex vertices[];
};
