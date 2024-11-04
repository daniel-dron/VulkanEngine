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

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <stb_image.h>
#include <utils/workers.h>
#include "loader.h"

#include <graphics/image_codex.h>
#include <graphics/light.h>
#include <graphics/r_resources.h>

#include <meshoptimizer.h>

#include "vk_engine.h"

#include <filesystem>

using namespace TL;
using namespace TL::renderer;

static std::vector<Material> LoadMaterials( const aiScene* scene, const std::string& basePath,
                                            std::vector<std::string>& externalTexturePaths ) {
    const auto n_materials = scene->mNumMaterials;

    std::vector<Material> materials;
    materials.reserve( n_materials );

    TL_Engine::Get( ).console.AddLog( "Loading {} materials", n_materials );

    for ( u32 i = 0; i < n_materials; i++ ) {
        Material   material;
        const auto ai_material = scene->mMaterials[i];

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
                const auto [texture, embeddedIndex] = scene->GetEmbeddedTextureAndIndex( texturePath.C_Str( ) );
                if ( embeddedIndex >= 0 ) {
                    // Embedded texture (GLB case)
                    textureId = embeddedIndex;
                }
                else {
                    // External texture (GLTF case)
                    auto it = std::find( externalTexturePaths.begin( ), externalTexturePaths.end( ),
                                         texturePath.C_Str( ) );
                    if ( it == externalTexturePaths.end( ) ) {
                        textureId = externalTexturePaths.size( );
                        externalTexturePaths.push_back( texturePath.C_Str( ) );
                    }
                    else {
                        textureId = std::distance( externalTexturePaths.begin( ), it );
                    }
                }
            }
            else {
                textureId = ImageCodex::InvalidImageId;
            }
        };

        // Load different texture types
        loadTexture( aiTextureType_DIFFUSE, material.ColorId );
        if ( material.ColorId == ImageCodex::InvalidImageId ) {
            loadTexture( aiTextureType_BASE_COLOR, material.ColorId );
        }

        loadTexture( aiTextureType_METALNESS, material.MetalRoughnessId );
        if ( material.MetalRoughnessId == ImageCodex::InvalidImageId ) {
            loadTexture( aiTextureType_SPECULAR, material.MetalRoughnessId );
        }

        loadTexture( aiTextureType_NORMALS, material.NormalId );

        materials.push_back( material );
    }

    return materials;
}

static std::vector<ImageId> LoadImages( TL_VkContext& gfx, const aiScene* scene, const std::string& basePath ) {
    std::vector<ImageId> images;
    images.resize( scene->mNumTextures );

    std::mutex gfxMutex;

    // Do not remove
    // WorkerPool destructor MUST be called before the mutex
    {
        WorkerPool pool( 20 );

        for ( u32 i = 0; i < scene->mNumTextures; i++ ) {
            auto        texture = scene->mTextures[i];
            std::string name    = texture->mFilename.C_Str( );

            pool.Work( [&gfx, &gfxMutex, &images, texture, name, i]( ) {
                i32 size = ( i32 )texture->mWidth;
                if ( texture->mHeight > 0 ) {
                    size = static_cast<unsigned long long>( texture->mWidth ) * texture->mHeight * sizeof( aiTexel );
                }

                int            width, height, channels;
                unsigned char* data = stbi_load_from_memory( reinterpret_cast<stbi_uc*>( texture->pcData ), size,
                                                             &width, &height, &channels, 4 );

                if ( data ) {
                    const VkExtent3D extent_3d = {
                            .width  = static_cast<uint32_t>( width ),
                            .height = static_cast<uint32_t>( height ),
                            .depth  = 1,
                    };

                    {
                        std::lock_guard<std::mutex> lock( gfxMutex );
                        images[i] = gfx.imageCodex.LoadImageFromData( name, data, extent_3d, VK_FORMAT_R8G8B8A8_UNORM,
                                                                      VK_IMAGE_USAGE_SAMPLED_BIT, true );
                    }

                    stbi_image_free( data );
                }
                else {
                    TL_Engine::Get( ).console.AddLog( "Failed to load embedded image {}\n\t{}", name,
                                                      stbi_failure_reason( ) );
                }

                TL_Engine::Get( ).console.AddLog( "Loaded Embedded Texture: {} {}", i, name );
            } );
        }
    }

    return images;
}

static std::vector<ImageId> LoadExternalImages( TL_VkContext& gfx, const std::vector<std::string>& paths,
                                                const std::string& basePath ) {
    std::vector<ImageId> images;
    images.resize( paths.size( ) );

    std::mutex gfxMutex;

    {
        WorkerPool pool( 20 );

        for ( u32 i = 0; i < paths.size( ); i++ ) {
            const auto& path     = paths[i];
            std::string fullPath = ( std::filesystem::path( basePath ) / path ).string( );

            pool.Work( [&gfx, &gfxMutex, &images, fullPath, i]( ) {
                int            width, height, channels;
                unsigned char* data = stbi_load( fullPath.c_str( ), &width, &height, &channels, 4 );

                if ( data ) {
                    const VkExtent3D extent_3d = {
                            .width  = static_cast<uint32_t>( width ),
                            .height = static_cast<uint32_t>( height ),
                            .depth  = 1,
                    };

                    {
                        std::lock_guard<std::mutex> lock( gfxMutex );
                        images[i] = gfx.imageCodex.LoadImageFromData(
                                fullPath, data, extent_3d, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true );
                    }

                    stbi_image_free( data );
                }
                else {
                    TL_Engine::Get( ).console.AddLog( "Failed to load external image {}\n\t{}", fullPath,
                                                      stbi_failure_reason( ) );
                }

                TL_Engine::Get( ).console.AddLog( "Loaded External Texture: {} {}", i, fullPath );
            } );
        }
    }

    return images;
}

void ProcessMaterials( std::vector<Material>& preprocessedMaterials, const std::vector<ImageId>& images,
                       TL_VkContext& gfx, const aiScene* aiScene ) {
    for ( auto& [base_color, metalness_factor, roughness_factor, color_id, metal_roughness_id, normal_id, name] :
          preprocessedMaterials ) {
        if ( color_id != ImageCodex::InvalidImageId ) {
            const auto texture = aiScene->mTextures[color_id];
            auto&      t       = gfx.imageCodex.GetImage( images.at( color_id ) );
            assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

            color_id = images.at( color_id );
        }

        if ( metal_roughness_id != ImageCodex::InvalidImageId ) {
            const auto texture = aiScene->mTextures[metal_roughness_id];
            auto&      t       = gfx.imageCodex.GetImage( images.at( metal_roughness_id ) );
            assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

            metal_roughness_id = images.at( metal_roughness_id );
        }

        if ( normal_id != ImageCodex::InvalidImageId ) {
            const auto texture = aiScene->mTextures[normal_id];
            auto&      t       = gfx.imageCodex.GetImage( images.at( normal_id ) );
            assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

            normal_id = images.at( normal_id );
        }
    }
}

static MeshHandle LoadMesh( TL_VkContext& gfx, aiMesh* aiMesh ) {
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

static std::vector<MeshHandle> LoadMeshes( TL_VkContext& gfx, const aiScene* scene ) {
    std::vector<MeshHandle> mesh_assets;

    for ( u32 i = 0; i < scene->mNumMeshes; i++ ) {
        auto mesh = LoadMesh( gfx, scene->mMeshes[i] );
        mesh_assets.push_back( mesh );
    }

    return mesh_assets;
}

static std::vector<MaterialHandle> UploadMaterials( TL_VkContext& gfx, const std::vector<Material>& materials ) {
    std::vector<MaterialHandle> gpu_materials;

    gpu_materials.reserve( materials.size( ) );
    for ( auto& material : materials ) {
        gpu_materials.push_back( gfx.MaterialPool.CreateMaterial( material ) );
    }

    return gpu_materials;
}

static std::vector<MeshAsset> MatchMaterialMeshes( const aiScene* ai_scene, const std::vector<MeshId>& meshes,
                                                   const std::vector<MaterialHandle>& materials ) {
    std::vector<MeshAsset> mesh_assets;

    for ( u32 meshIndex = 0; meshIndex < ai_scene->mNumMeshes; meshIndex++ ) {
        const auto mat_idx = ai_scene->mMeshes[meshIndex]->mMaterialIndex;
        mesh_assets.push_back( { meshIndex, mat_idx } );
    }

    return mesh_assets;
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

static std::shared_ptr<Node> LoadNode( Scene& scene, const aiScene* ai_scene, const aiNode* node ) {
    auto sceneNode = std::make_shared<Node>( );

    sceneNode->Name = node->mName.C_Str( );
    TL_Engine::Get( ).console.AddLog( "{}", sceneNode->Name.c_str( ) );

    auto transform = AssimpToGlm( node->mTransformation );
    if ( node->mTransformation == aiMatrix4x4( ) ) {
        transform = glm::identity<Mat4>( );
    }

    sceneNode->SetTransform( transform );

    if ( node->mNumMeshes == 0 ) {
        sceneNode->MeshAssets.clear( );
    }
    else {
        for ( u32 i = 0; i < node->mNumMeshes; i++ ) {
            const auto  mesh = node->mMeshes[i];
            const auto& aabb = ai_scene->mMeshes[mesh]->mAABB;

            AABoundingBox bounding_box = {
                    .min = { aabb.mMin.x, aabb.mMin.y, aabb.mMin.z },
                    .max = { aabb.mMax.x, aabb.mMax.y, aabb.mMax.z },
            };

            sceneNode->BoundingBoxes.push_back( std::move( bounding_box ) );

            MeshAsset mesh_asset;
            mesh_asset.MaterialIndex = ai_scene->mMeshes[mesh]->mMaterialIndex;
            mesh_asset.MeshIndex     = mesh;

            sceneNode->MeshAssets.push_back( mesh_asset );
        }
    }

    for ( u32 i = 0; i < node->mNumChildren; i++ ) {
        auto child    = LoadNode( scene, ai_scene, node->mChildren[i] );
        child->Parent = sceneNode;
        sceneNode->Children.push_back( child );
        scene.AllNodes.push_back( child );
    }

    return sceneNode;
}

static void LoadHierarchy( const aiScene* ai_scene, Scene& scene ) {
    const auto root_node = ai_scene->mRootNode;

    auto node = LoadNode( scene, ai_scene, root_node );
    scene.TopNodes.push_back( node );
    scene.AllNodes.push_back( node );
}

static void LoadCameras( const aiScene* aiScene, Scene& scene ) {
    if ( !aiScene->HasCameras( ) ) {
        return;
    }

    for ( unsigned int i = 0; i < aiScene->mNumCameras; ++i ) {
        auto ai_camera = aiScene->mCameras[i];

        // find its node
        auto node = scene.FindNodeByName( ai_camera->mName.C_Str( ) );
        if ( node ) {
            auto transform = node->GetTransformMatrix( );

            auto position = Vec3( ai_camera->mPosition.x, ai_camera->mPosition.y, ai_camera->mPosition.z );
            position      = transform * Vec4( position, 1.0f );

            Vec3 look_at( ai_camera->mLookAt.x, ai_camera->mLookAt.y, ai_camera->mLookAt.z );
            look_at        = transform * Vec4( look_at, 0.0f );
            Vec3 direction = glm::normalize( look_at - position );

            float yaw   = atan2( direction.z, direction.x );
            float pitch = asin( direction.y );
            yaw         = glm::degrees( yaw );
            pitch       = glm::degrees( pitch );

            Camera camera = { position, yaw, pitch, WIDTH, HEIGHT };
            scene.Cameras.push_back( camera );
        }
    }
}

void RgBtoHsv( const float fR, const float fG, const float fB, float& fH, float& fS, float& fV ) {
    const float f_c_max = std::max( std::max( fR, fG ), fB );
    const float f_c_min = std::min( std::min( fR, fG ), fB );
    const float f_delta = f_c_max - f_c_min;

    if ( f_delta > 0 ) {
        if ( f_c_max == fR ) {
            fH = 60 * ( ( f32 )fmod( ( ( fG - fB ) / f_delta ), 6 ) );
        }
        else if ( f_c_max == fG ) {
            fH = 60 * ( ( ( fB - fR ) / f_delta ) + 2 );
        }
        else if ( f_c_max == fB ) {
            fH = 60 * ( ( ( fR - fG ) / f_delta ) + 4 );
        }

        if ( f_c_max > 0 ) {
            fS = f_delta / f_c_max;
        }
        else {
            fS = 0;
        }

        fV = f_c_max;
    }
    else {
        fH = 0;
        fS = 0;
        fV = f_c_max;
    }

    if ( fH < 0 ) {
        fH = 360 + fH;
    }
}

float ConvertHueToImGui( const float hue, const float sourceMax = 360.0f ) {
    const float normalized_hue = hue / sourceMax;
    return std::clamp( normalized_hue, 0.0f, 1.0f );
}

void ConvertHsvToImGui( float& h, float& s, float& v, float sourceHueMax = 360.0f ) {
    h = ConvertHueToImGui( h, sourceHueMax );
    s = std::clamp( s, 0.0f, 1.0f );
    v = std::clamp( v, 0.0f, 1.0f );
}

static void LoadLights( TL_VkContext& gfx, const aiScene* aiScene, Scene& scene ) {
    if ( !aiScene->HasLights( ) ) {
        return;
    }

    for ( unsigned int i = 0; i < aiScene->mNumLights; i++ ) {
        const auto ai_light = aiScene->mLights[i];
        const auto node     = scene.FindNodeByName( ai_light->mName.C_Str( ) );

        if ( !node ) {
            continue;
        }

        if ( ai_light->mType == aiLightSource_POINT ) {
            PointLight light{ };
            light.node = node.get( );

            RgBtoHsv( ai_light->mColor.r, ai_light->mColor.g, ai_light->mColor.b, light.hsv.hue, light.hsv.saturation,
                      light.hsv.value );
            ConvertHsvToImGui( light.hsv.hue, light.hsv.saturation, light.hsv.value );
            light.power = ( ai_light->mPower / 683.0f ) * 4.0f * 3.14159265359f;

            light.constant  = ai_light->mAttenuationConstant;
            light.linear    = ai_light->mAttenuationLinear;
            light.quadratic = ai_light->mAttenuationQuadratic;

            scene.PointLights.emplace_back( light );
        }
        else if ( ai_light->mType == aiLightSource_DIRECTIONAL ) {
            DirectionalLight light;
            light.node = node.get( );

            RgBtoHsv( ai_light->mColor.r, ai_light->mColor.g, ai_light->mColor.b, light.hsv.hue, light.hsv.saturation,
                      light.hsv.value );
            ConvertHsvToImGui( light.hsv.hue, light.hsv.saturation, light.hsv.value );
            light.power = ( ai_light->mPower / 683.0f );

            light.shadowMap = gfx.imageCodex.CreateEmptyImage(
                    "shadowmap", VkExtent3D{ 2048, 2048, 1 }, VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false );

            scene.DirectionalLights.emplace_back( light );
        }
    }
}

std::unique_ptr<Scene> GltfLoader::Load( TL_VkContext& gfx, const std::string& path ) {
    auto  scene_ptr = std::make_unique<Scene>( );
    auto& scene     = *scene_ptr;

    scene.name           = path;
    std::string basePath = std::filesystem::path( path ).parent_path( ).string( );

    Assimp::Importer importer;
    const auto       ai_scene =
            importer.ReadFile( path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipUVs |
                                             aiProcess_FlipWindingOrder | aiProcess_GenBoundingBoxes );

    TL_Engine::Get( ).console.AddLog( "Loading meshes..." );
    const std::vector<MeshHandle> meshes = LoadMeshes( gfx, ai_scene );

    TL_Engine::Get( ).console.AddLog( "Loading materials..." );
    std::vector<std::string> externalTexturePaths;
    std::vector<Material>    materials = LoadMaterials( ai_scene, basePath, externalTexturePaths );

    TL_Engine::Get( ).console.AddLog( "Loading images..." );
    std::vector<ImageId> embeddedImages = LoadImages( gfx, ai_scene, basePath );
    std::vector<ImageId> externalImages = LoadExternalImages( gfx, externalTexturePaths, basePath );

    TL_Engine::Get( ).console.AddLog( "Processing materials..." );
    // Update material IDs based on whether they were embedded or external
    for ( auto& material : materials ) {
        if ( material.ColorId != ImageCodex::InvalidImageId ) {
            material.ColorId = material.ColorId < embeddedImages.size( )
                                     ? embeddedImages[material.ColorId]
                                     : externalImages[material.ColorId - embeddedImages.size( )];
        }
        if ( material.MetalRoughnessId != ImageCodex::InvalidImageId ) {
            material.MetalRoughnessId = material.MetalRoughnessId < embeddedImages.size( )
                                              ? embeddedImages[material.MetalRoughnessId]
                                              : externalImages[material.MetalRoughnessId - embeddedImages.size( )];
        }
        if ( material.NormalId != ImageCodex::InvalidImageId ) {
            material.NormalId = material.NormalId < embeddedImages.size( )
                                      ? embeddedImages[material.NormalId]
                                      : externalImages[material.NormalId - embeddedImages.size( )];
        }
    }

    const auto gpu_materials = UploadMaterials( gfx, materials );
    scene.Materials          = gpu_materials;
    scene.Meshes             = meshes;

    LoadHierarchy( ai_scene, scene );
    LoadCameras( ai_scene, scene );
    LoadLights( gfx, ai_scene, scene );

    return std::move( scene_ptr );
}