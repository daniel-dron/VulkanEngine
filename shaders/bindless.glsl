#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_image_load_formatted : require

layout (set = 0, binding = 0) uniform texture2D textures[];
layout (set = 0, binding = 0) uniform texture2DMS texturesMS[];
layout (set = 0, binding = 0) uniform textureCube textureCubes[];
layout (set = 0, binding = 0) uniform texture2DArray textureArrays[];
layout (set = 0, binding = 1) uniform sampler samplers[];
layout (set = 0, binding = 2) uniform image2D storageImages[];
layout (set = 0, binding = 2) uniform imageCube storageCubemapImages[];

#define NEAREST_SAMPLER_ID 0
#define LINEAR_SAMPLER_ID  1
#define SHADOW_SAMPLER_ID  2

vec4 sampleTexture2DNearest(uint texID, vec2 uv) {
    return texture(nonuniformEXT(sampler2D(textures[texID], samplers[NEAREST_SAMPLER_ID])), uv);
}

vec4 sampleTexture2DMSNearest(uint texID, ivec2 p, int s) {
    return texelFetch(nonuniformEXT(sampler2DMS(texturesMS[texID], samplers[NEAREST_SAMPLER_ID])), p, s);
}

vec4 sampleTexture2DLinear(uint texID, vec2 uv) {
    return texture(nonuniformEXT(sampler2D(textures[texID], samplers[LINEAR_SAMPLER_ID])), uv);
}

vec4 sampleTextureCubeNearest(uint texID, vec3 p) {
    return texture(nonuniformEXT(samplerCube(textureCubes[texID], samplers[NEAREST_SAMPLER_ID])), p);
}

vec4 sampleTextureCubeLinear(uint texID, vec3 p) {
    return texture(nonuniformEXT(samplerCube(textureCubes[texID], samplers[LINEAR_SAMPLER_ID])), p);
}

float sampleTextureArrayShadow(uint texID, vec4 p) {
    return texture(nonuniformEXT(sampler2DArrayShadow(textureArrays[texID], samplers[SHADOW_SAMPLER_ID])), p);
}

ivec2 GetStorageImageSize(uint tex_id) {
    return imageSize(nonuniformEXT(storageImages[tex_id]));
}

void StoragePixelAt(uint tex_id, ivec2 pos, vec4 color) {
    imageStore(nonuniformEXT(storageImages[tex_id]), pos, color);
}

void StorageCubemapPixelAt(uint tex_id, ivec3 pos, vec4 color) {
    imageStore(nonuniformEXT(storageCubemapImages[tex_id]), pos, color);
}
