#include "loader.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <graphics/material_codex.h>
#include <vk_engine.h>
#include <utils/workers.h>
#include <stb_image.h>

static std::vector<Material> loadMaterials( const GfxDevice& gfx, const aiScene* scene ) {
	const auto n_materials = scene->mNumMaterials;

	std::vector<Material> materials;
	materials.reserve( n_materials );

	fmt::println( "Loading {} materials", n_materials );

	for ( auto i = 0; i < n_materials; i++ ) {
		Material material;
		auto ai_material = scene->mMaterials[i];

		material.name = ai_material->GetName( ).C_Str( );

		aiColor4D base_diffuse_color{};
		ai_material->Get( AI_MATKEY_COLOR_DIFFUSE, base_diffuse_color );
		material.base_color = { base_diffuse_color.r, base_diffuse_color.g, base_diffuse_color.b, base_diffuse_color.a };

		float metalness_factor;
		ai_material->Get( AI_MATKEY_METALLIC_FACTOR, metalness_factor );
		material.metalness_factor = metalness_factor;

		float roughness_factor;
		ai_material->Get( AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor );
		material.roughness_factor = roughness_factor;

		aiString diffuse_path;
		if ( aiReturn_SUCCESS == ai_material->GetTexture( aiTextureType_DIFFUSE, 0, &diffuse_path ) ||
			aiReturn_SUCCESS == ai_material->GetTexture( aiTextureType_BASE_COLOR, 0, &diffuse_path ) ) {
			auto r = scene->GetEmbeddedTextureAndIndex( diffuse_path.C_Str( ) );
			material.color_id = r.second;
		} else {
			material.color_id = ImageCodex::INVALID_IMAGE_ID;
		}

		aiString metal_roughness_path;
		if ( aiReturn_SUCCESS == ai_material->GetTexture( aiTextureType_METALNESS, 0, &metal_roughness_path ) ||
			aiReturn_SUCCESS == ai_material->GetTexture( aiTextureType_SPECULAR, 0, &metal_roughness_path ) ) {
			auto r = scene->GetEmbeddedTextureAndIndex( metal_roughness_path.C_Str( ) );
			material.metal_roughness_id = r.second;
		} else {
			material.metal_roughness_id = ImageCodex::INVALID_IMAGE_ID;
		}

		aiString normal_path;
		if ( aiReturn_SUCCESS == ai_material->GetTexture( aiTextureType_NORMALS, 0, &normal_path ) ) {
			auto r = scene->GetEmbeddedTextureAndIndex( normal_path.C_Str( ) );
			material.normal_id = r.second;
		} else {
			material.normal_id = ImageCodex::INVALID_IMAGE_ID;
		}

		materials.push_back( material );
	}

	return materials;
}

static std::vector<ImageID> loadImages( GfxDevice& gfx, const aiScene* scene ) {
	std::vector<ImageID> images;
	images.resize( scene->mNumTextures );

	std::mutex gfx_mutex;

	// Do not remove
	// WorkerPool destructor MUST be called before the mutex
	{
		WorkerPool pool( 20 );

		for ( auto i = 0; i < scene->mNumTextures; i++ ) {
			auto texture = scene->mTextures[i];
			std::string name = texture->mFilename.C_Str( );

			pool.work( [&gfx, &gfx_mutex, &images, texture, name, i]( ) {
				size_t size = texture->mWidth;
				if ( texture->mHeight > 0 ) {
					size = static_cast<unsigned long long>(texture->mWidth) * texture->mHeight * sizeof( aiTexel );
				}

				int width, height, channels;
				unsigned char* data = stbi_load_from_memory( (stbi_uc*)texture->pcData, size, &width, &height, &channels, 4 );

				if ( data ) {
					VkExtent3D size = {
						.width = (uint32_t)(width),
						.height = (uint32_t)(height),
						.depth = 1
					};

					{
						// TODO: use a staged pool for batched gpu loading
						std::lock_guard<std::mutex> lock( gfx_mutex );
						images[i] = gfx.image_codex.loadImageFromData( name, data, size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true );
					}

					stbi_image_free( data );
				} else {
					fmt::println( "Failed to load image {}\n\t{}", name, stbi_failure_reason( ) );
				}

				fmt::println( "Loaded Texture: {} {}", i, name );
			} );
		}
	}

	return images;
}

void processMaterials( std::vector<Material>& preprocessed_materials, std::vector <ImageID> images, GfxDevice& gfx, const aiScene* ai_scene ) {
	for ( auto& material : preprocessed_materials ) {
		if ( material.color_id != ImageCodex::INVALID_IMAGE_ID ) {
			auto texture = ai_scene->mTextures[material.color_id];
			auto& t = gfx.image_codex.getImage( images.at( material.color_id ) );
			assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

			material.color_id = images.at( material.color_id );
		}

		if ( material.metal_roughness_id != ImageCodex::INVALID_IMAGE_ID ) {
			auto texture = ai_scene->mTextures[material.metal_roughness_id];
			auto& t = gfx.image_codex.getImage( images.at( material.metal_roughness_id ) );
			assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

			material.metal_roughness_id = images.at( material.metal_roughness_id );
		}

		if ( material.normal_id != ImageCodex::INVALID_IMAGE_ID ) {
			auto texture = ai_scene->mTextures[material.normal_id];
			auto& t = gfx.image_codex.getImage( images.at( material.normal_id ) );
			assert( strcmp( texture->mFilename.C_Str( ), t.GetName( ).c_str( ) ) == 0 && "missmatched texture" );

			material.normal_id = images.at( material.normal_id );
		}
	}
}

static MeshID loadMesh( GfxDevice& gfx, const aiScene* scene, aiMesh* ai_mesh ) {
	Mesh mesh;
	mesh.vertices.clear( );
	mesh.indices.clear( );

	mesh.vertices.reserve( ai_mesh->mNumVertices );
	for ( auto i = 0; i < ai_mesh->mNumVertices; i++ ) {
		Mesh::Vertex vertex{};
		vertex.position = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };

		vertex.normal = { ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };

		vertex.uv_x = ai_mesh->mTextureCoords[0][i].x;
		vertex.uv_y = ai_mesh->mTextureCoords[0][i].y;

		vertex.tangent = { ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z };
		vertex.bitangent = { ai_mesh->mBitangents[i].x, ai_mesh->mBitangents[i].y, ai_mesh->mBitangents[i].z };
		mesh.vertices.push_back( vertex );
	}

	mesh.indices.reserve( ai_mesh->mNumFaces * 3 );
	for ( auto i = 0; i < ai_mesh->mNumFaces; i++ ) {
		auto face = ai_mesh->mFaces[i];
		mesh.indices.push_back( face.mIndices[0] );
		mesh.indices.push_back( face.mIndices[1] );
		mesh.indices.push_back( face.mIndices[2] );
	}

	mesh.aabb = {
		.min = vec3{ai_mesh->mAABB.mMin.x, ai_mesh->mAABB.mMin.y, ai_mesh->mAABB.mMin.z },
		.max = vec3{ai_mesh->mAABB.mMax.x, ai_mesh->mAABB.mMax.y, ai_mesh->mAABB.mMax.z },
	};

	return gfx.mesh_codex.addMesh( gfx, mesh );
}

static std::vector<MeshID> loadMeshes( GfxDevice& gfx, const aiScene* scene ) {
	std::vector<MeshID> mesh_assets;

	for ( auto i = 0; i < scene->mNumMeshes; i++ ) {
		auto mesh = loadMesh( gfx, scene, scene->mMeshes[i] );
		mesh_assets.push_back( mesh );
	}

	return mesh_assets;
}

static std::vector<MaterialID> uploadMaterials( GfxDevice& gfx, const std::vector<Material>& materials ) {
	std::vector<MaterialID> gpu_materials;

	for ( auto& material : materials ) {
		gpu_materials.push_back( gfx.material_codex.addMaterial( gfx, material ) );
	}

	return gpu_materials;
}

static std::vector<Scene::MeshAsset> matchMaterialMeshes( const aiScene* scene, const std::vector<MeshID>& meshes, const std::vector<MaterialID>& materials ) {
	std::vector<Scene::MeshAsset> mesh_assets;

	for ( auto i = 0; i < scene->mNumMeshes; i++ ) {
		auto mat_idx = materials[scene->mMeshes[i]->mMaterialIndex];
		mesh_assets.push_back( { meshes.at( i ), mat_idx } );
	}

	return mesh_assets;
}

inline glm::mat4 assimpToGLM( const aiMatrix4x4& from ) {
	glm::mat4 to{};
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

static std::shared_ptr<Scene::Node> loadNode( const aiNode* node ) {
	auto scene_node = std::make_shared<Scene::Node>( );

	scene_node->name = node->mName.C_Str( );

	auto transform = assimpToGLM( node->mTransformation );
	if ( node->mTransformation == aiMatrix4x4( ) ) {
		transform = glm::identity<mat4>( );
	}

	scene_node->setTransform( transform );

	if ( node->mNumMeshes == 0 ) {
		scene_node->mesh_ids.clear( );
	} else {
		for ( auto i = 0; i < node->mNumMeshes; i++ ) {
			auto mesh = node->mMeshes[i];
			scene_node->mesh_ids.push_back( mesh );
		}
	}

	for ( auto i = 0; i < node->mNumChildren; i++ ) {
		auto child = loadNode( node->mChildren[i] );
		child->parent = scene_node;
		scene_node->children.push_back( child );
	}

	return scene_node;
}

static void loadHierarchy( const aiScene* ai_scene, Scene& scene ) {
	auto root_node = ai_scene->mRootNode;

	scene.top_nodes.push_back( loadNode( root_node ) );
}

static void loadCameras( const aiScene* ai_scene, Scene& scene ) {
	if ( !ai_scene->HasCameras( ) ) {
		return;
	}

	for ( unsigned int i = 0; i < ai_scene->mNumCameras; ++i ) {
		auto ai_camera = ai_scene->mCameras[i];

		auto position = vec3( ai_camera->mPosition.x, ai_camera->mPosition.y, ai_camera->mPosition.z );
		glm::vec3 look_at( ai_camera->mLookAt.x, ai_camera->mLookAt.y, ai_camera->mLookAt.z );
		glm::vec3 direction = glm::normalize( look_at - position );

		float yaw = atan2( direction.z, direction.x );
		float pitch = asin( direction.y );
		yaw = glm::degrees( yaw );
		pitch = glm::degrees( pitch );

		Camera camera = { position, yaw, pitch, 0, 0 };
		scene.cameras.push_back( camera );
	}
}

std::unique_ptr<Scene> GltfLoader::load( GfxDevice& gfx, const std::string& path ) {
	auto scene_ptr = std::make_unique<Scene>( );
	auto& scene = *scene_ptr;

	scene.name = path;

	Assimp::Importer importer;
	const auto ai_scene = importer.ReadFile( path,
		aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipUVs |
		aiProcess_FlipWindingOrder | aiProcess_GenBoundingBoxes | aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes );

	fmt::println( "Loading meshes..." );
	std::vector<MeshID> meshes = loadMeshes( gfx, ai_scene );

	fmt::println( "Loading materials..." );
	std::vector<Material> materials = loadMaterials( gfx, ai_scene );

	fmt::println( "Loading images..." );
	std::vector<ImageID> images = loadImages( gfx, ai_scene );

	fmt::println( "Matching materials..." );
	processMaterials( materials, images, gfx, ai_scene );

	auto gpu_materials = uploadMaterials( gfx, materials );
	scene.materials = gpu_materials;

	auto mesh_assets = matchMaterialMeshes( ai_scene, meshes, gpu_materials );
	scene.meshes = mesh_assets;

	loadHierarchy( ai_scene, scene );

	loadCameras( ai_scene, scene );

	return std::move( scene_ptr );
}
