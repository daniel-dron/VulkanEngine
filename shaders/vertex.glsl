struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec3 tangent;
	float padding;
	vec3 bitangent;
	float padding2;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};