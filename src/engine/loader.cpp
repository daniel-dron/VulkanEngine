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

#include <cstdint>
#include <pch.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <stb_image.h>
#include <string>
#include <unordered_map>
#include <utils/workers.h>
#include "loader.h"

#include <graphics/light.h>
#include <graphics/resources/r_image.h>
#include <graphics/resources/r_resources.h>

#include "engine/scene.h"
#include "fmt/core.h"
#include "graphics/utils/vk_types.h"
#include "tl_engine.h"
#include "world/tl_components.h"
#include "world/tl_entity.h"
#include "world/tl_scene.h"
#include <filesystem>

using namespace TL;
using namespace TL::renderer;
using namespace TL::world;
using namespace std::filesystem;

static ImageId LoadEmbeddedTexture( const aiScene* aiScene, i32 embeddedIndex ) {
    assert( false && "Unimplemented" );
    return 0;
}

inline glm::mat4 AssimpToGlm( const aiMatrix4x4& from ) {
    glm::mat4 to{ };
    to[0][0] = from.a1;
    to[1][0] = from.a2;
    to[2][0] = from.a3;
    to[3][0] = from.a4;
    to[0][1] = from.b1;
    to[1][1] = from.b2;
    to[2][1] = from.b3;
    to[3][1] = from.b4;
    to[0][2] = from.c1;
    to[1][2] = from.c2;
    to[2][2] = from.c3;
    to[3][2] = from.c4;
    to[0][3] = from.d1;
    to[1][3] = from.d2;
    to[2][3] = from.d3;
    to[3][3] = from.d4;
    return to;
}

static ImageId LoadExternalTexture( const aiScene* aiScene, const path& path ) {
    fmt::println( "Loading texture: {}", path.string( ).c_str( ) );

    ImageId id = -1;

    int            width, height, channels;
    unsigned char* data = stbi_load( path.string( ).c_str( ), &width, &height, &channels, 4 );

    if ( data ) {
        const VkExtent3D extent_3d = {
                .width  = static_cast<uint32_t>( width ),
                .height = static_cast<uint32_t>( height ),
                .depth  = 1 };

        id = vkctx->ImageCodex.LoadImageFromData( path.string( ), data, extent_3d, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true );
        stbi_image_free( data );
    }
    else {
        TL_Engine::Get( ).console.AddLog( "Failed to load external image {}\n\t{}", path.string( ).c_str( ), stbi_failure_reason( ) );
    }

    return id;
}

static std::vector<MaterialHandle> LoadMaterials( const aiScene* aiScene, const std::string& basePath ) {
    const auto n_materials = aiScene->mNumMaterials;

    std::vector<MaterialHandle> materials;
    materials.reserve( n_materials );

    std::unordered_map<std::string, ImageId> cached_external_ids;
    std::unordered_map<i32, ImageId>         cached_embedded_ids;

    TL_Engine::Get( ).console.AddLog( "Loading {} materials", n_materials );

    for ( u32 i = 0; i < n_materials; i++ ) {
        Material   material;
        const auto ai_material = aiScene->mMaterials[i];

        material.Name = ai_material->GetName( ).C_Str( );

        aiColor4D base_diffuse_color{ };
        ai_material->Get( AI_MATKEY_COLOR_DIFFUSE, base_diffuse_color );
        material.BaseColor = { base_diffuse_color.r, base_diffuse_color.g, base_diffuse_color.b, base_diffuse_color.a };

        float metalness_factor;
        ai_material->Get( AI_MATKEY_METALLIC_FACTOR, metalness_factor );
        material.MetalnessFactor = metalness_factor;

        float roughness_factor;
        ai_material->Get( AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor );
        material.roughnessFactor = roughness_factor;

        auto loadTexture = [&]( aiTextureType type, ImageId& textureId ) {
            aiString texturePath;
            if ( aiReturn_SUCCESS == ai_material->GetTexture( type, 0, &texturePath ) ) {
                const auto [texture, embeddedIndex] = aiScene->GetEmbeddedTextureAndIndex( texturePath.C_Str( ) );
                if ( embeddedIndex >= 0 ) {
                    // Embedded texture (GLB case)
                    // Check if previously loaded
                    if ( cached_embedded_ids.contains( embeddedIndex ) ) {
                        fmt::println( "Found {} on embedded images cache", embeddedIndex );
                        textureId = cached_embedded_ids.at( embeddedIndex );
                    }
                    else {
                        textureId = LoadEmbeddedTexture( aiScene, embeddedIndex );

                        cached_embedded_ids[embeddedIndex] = textureId;
                    }
                }
                else {
                    // External texture (GLTF case)
                    if ( cached_external_ids.contains( texturePath.C_Str( ) ) ) {
                        fmt::println( "Found {} on external images cache", texturePath.C_Str( ) );
                        textureId = cached_external_ids.at( texturePath.C_Str( ) );
                    }
                    else {
                        textureId = LoadExternalTexture( aiScene, path( basePath ) / texturePath.C_Str( ) );

                        cached_external_ids[texturePath.C_Str( )] = textureId;
                    }
                }
            }
            else {
                textureId = ImageCodex::InvalidImageId;
            }
        };

        // Load different texture types
        // loadTexture( aiTextureType_DIFFUSE, material.ColorId );
        // if ( material.ColorId == ImageCodex::InvalidImageId ) {
        //     loadTexture( aiTextureType_BASE_COLOR, material.ColorId );
        // }

        // loadTexture( aiTextureType_METALNESS, material.MetalRoughnessId );
        // if ( material.MetalRoughnessId == ImageCodex::InvalidImageId ) {
        //     loadTexture( aiTextureType_SPECULAR, material.MetalRoughnessId );
        // }

        // loadTexture( aiTextureType_NORMALS, material.NormalId );

        material.ColorId          = ImageCodex::InvalidImageId;
        material.MetalRoughnessId = ImageCodex::InvalidImageId;
        material.NormalId         = ImageCodex::InvalidImageId;

        auto handle = vkctx->MaterialPool.CreateMaterial( material );
        materials.push_back( handle );
    }

    return materials;
}

static MeshHandle LoadMesh( aiMesh* aiMesh ) {
    MeshContent mesh;
    mesh.Vertices.clear( );
    mesh.Indices.clear( );

    mesh.Vertices.reserve( aiMesh->mNumVertices );
    for ( u32 i = 0; i < aiMesh->mNumVertices; i++ ) {
        Vertex vertex{ };

        Vec2 uvs = { };
        if ( aiMesh->mTextureCoords[0] ) {
            uvs.x = aiMesh->mTextureCoords[0][i].x;
            uvs.y = aiMesh->mTextureCoords[0][i].y;
        }

        vertex.Position = { aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z, uvs.x };
        vertex.Normal   = { aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z, uvs.y };

        if ( aiMesh->mTangents != nullptr && aiMesh->mBitangents != nullptr ) {
            vertex.Tangent   = { aiMesh->mTangents[i].x, -aiMesh->mTangents[i].y, aiMesh->mTangents[i].z, 0.0f };
            vertex.Bitangent = { aiMesh->mBitangents[i].x, -aiMesh->mBitangents[i].y, aiMesh->mBitangents[i].z, 0.0f };
        }
        mesh.Vertices.push_back( vertex );
    }

    std::vector<uint32_t> indices;
    indices.reserve( aiMesh->mNumFaces * 3 );
    for ( u32 i = 0; i < aiMesh->mNumFaces; i++ ) {
        auto& face = aiMesh->mFaces[i];
        indices.push_back( face.mIndices[0] );
        indices.push_back( face.mIndices[1] );
        indices.push_back( face.mIndices[2] );
    }
    mesh.Indices = indices;

    mesh.Aabb = {
            .min = Vec3{ aiMesh->mAABB.mMin.x, aiMesh->mAABB.mMin.y, aiMesh->mAABB.mMin.z },
            .max = Vec3{ aiMesh->mAABB.mMax.x, aiMesh->mAABB.mMax.y, aiMesh->mAABB.mMax.z } };

    return vkctx->MeshPool.CreateMesh( mesh );
}

static std::vector<MeshHandle> LoadMeshes( const aiScene* aiScene ) {
    std::vector<MeshHandle> mesh_assets;

    for ( u32 i = 0; i < aiScene->mNumMeshes; i++ ) {
        auto mesh = LoadMesh( aiScene->mMeshes[i] );
        mesh_assets.push_back( mesh );
    }

    return mesh_assets;
}

static void LoadNode( const aiScene* aiScene, const aiNode* node, World& world, EntityHandle entityHandle, const std::vector<MeshHandle>& meshes, const std::vector<MaterialHandle>& materials ) {
    // Load node into entity
    auto entity = world.GetEntity( entityHandle ).value( );

    // Set Renderable component if node has a mesh
    if ( node->mNumMeshes > 0 ) {
        auto mesh_id = node->mMeshes[0];
        auto mesh    = aiScene->mMeshes[mesh_id];
        entity->AddComponent<TL::world::Renderable>( meshes[mesh_id], materials[mesh->mMaterialIndex] );
    }

    // Set transform
    auto transform = AssimpToGlm( node->mTransformation );
    if ( node->mTransformation == aiMatrix4x4( ) ) {
        transform = glm::identity<Mat4>( );
    }
    entity->SetTransform( transform );

    // For each child node, create a child entity and call LoadNode on it
    for ( u32 i = 0; i < node->mNumChildren; i++ ) {
        auto child_node          = node->mChildren[i];
        auto child_entity_handle = world.CreateEntity( child_node->mName.C_Str( ), entityHandle );
        LoadNode( aiScene, child_node, world, child_entity_handle, meshes, materials );
    }
}

static void LoadHierarchy( const aiScene* aiScene, World& world, Entity* entity, const std::vector<MeshHandle>& meshes, const std::vector<MaterialHandle>& materials ) {
    const auto root_node = aiScene->mRootNode;

    auto childEntityHandle = world.CreateEntity( root_node->mName.C_Str( ), entity->GetHandle( ) );
    LoadNode( aiScene, root_node, world, childEntityHandle, meshes, materials );
}

void GltfLoader::LoadWorldFromGltf( const std::string& path, World& world, EntityHandle entityHandle ) {
    assert( !path.empty( ) && "Empty path" );
    assert( entityHandle != INVALID_ENTITY && "Invalid entity" );
    assert( world.IsValidEntity( entityHandle ) && "Invalid entity" );

    auto gltf_exists = exists( path );
    if ( !gltf_exists ) {
        fmt::println( "File {} does not exist!", path.c_str( ) );
        return;
    }

    Assimp::Importer importer;

    const auto  ai_scene  = importer.ReadFile( path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_FlipWindingOrder | aiProcess_GenBoundingBoxes );
    std::string base_path = std::filesystem::path( path ).parent_path( ).string( );

    auto meshes    = LoadMeshes( ai_scene );
    auto materials = LoadMaterials( ai_scene, base_path );

    if ( auto entity = world.GetEntity( entityHandle ).value( ) ) {
        LoadHierarchy( ai_scene, world, entity, meshes, materials );
    }

    fmt::println( "Loaded {} meshes", meshes.size( ) );
}