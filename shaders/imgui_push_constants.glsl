#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_buffer_reference : require

struct Vertex {
    vec2 position;
    vec2 uvs;
    uint color;
};

layout( buffer_reference, scalar ) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout( push_constant ) uniform constants {
    VertexBuffer vertex_buffer;
    uint texture_id;
    uint is_srgb;
    vec2 offset;
    vec2 scale;
} pc;