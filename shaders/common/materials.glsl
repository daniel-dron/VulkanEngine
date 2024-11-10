#extension GL_EXT_buffer_reference : require

struct Material {
    vec4    BaseColor;          // 16 bytes
    vec4    EmissiveColor;      // 16 bytes
    vec4    Factors;            // 16 bytes ( metalness, roughness, N/A, N/A )
    int     TextureIndices[4];  // 16 bytes ( albedo, mr, normal, N/A )
};

layout (buffer_reference, scalar) readonly buffer MaterialsBuffer {
    Material mat[];
} materialsBuffer;