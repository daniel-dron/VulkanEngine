#include "../common/vertex.glsl"

struct PerDrawData {
	mat4 			WorldFromLocal;
	VertexBuffer	VertexBufferAddress;
	int             MaterialId;
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer PerDrawDataList {
	PerDrawData datas[];
};

layout( push_constant, scalar ) uniform constants {
	SceneBuffer scene;
	PerDrawDataList draw_data;
} pc;