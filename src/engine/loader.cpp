#include "loader.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <stb_image.h>

#include <engine/scene.h>
#include <graphics/gfx_device.h>
#include <graphics/image_codex.h>
#include <graphics/mesh_codex.h>

#include <memory>
#include <chrono>
#include <future>
#include <thread>
#include <chrono>

static std::optional<fastgltf::Asset> loadFromFile( fastgltf::GltfDataBuffer& data, const std::string& path, fastgltf::Options options ) {
	auto type = fastgltf::determineGltfFileType( &data );
	std::filesystem::path file_path = path;
	fastgltf::Parser parser;

	switch ( type ) {
	case fastgltf::GltfType::glTF:
	{
		auto load = parser.loadGLTF( &data, file_path.parent_path( ), options );
		if ( load ) {
			return std::move( load.get( ) );
		} else {
			return {};
		}
		break;
	}

	case fastgltf::GltfType::GLB:
	{
		auto load = parser.loadBinaryGLTF( &data, file_path.parent_path( ), options );
		if ( load ) {
			return std::move( load.get( ) );
		} else {
			return {};
		}
		break;
	}
	default:
		return {};
	}
}


static std::string imageMimeToStr( fastgltf::MimeType mime ) {
	switch ( mime ) {
	case fastgltf::MimeType::DDS:
		return "DDS";
	case fastgltf::MimeType::GltfBuffer:
		return "GltfBuffer";
	case fastgltf::MimeType::JPEG:
		return "JPEG";
	case fastgltf::MimeType::KTX2:
		return "KTX2";
	case fastgltf::MimeType::None:
		return "NONE";
	case fastgltf::MimeType::OctetStream:
		return "OctetStream";
	case fastgltf::MimeType::PNG:
		return "PNG";
	default:
		return "UNKNOWN";
	}
};

static ImageID loadImage( GfxDevice& gfx, fastgltf::Asset& asset, fastgltf::Image& image ) {
	int width = 0;
	int height = 0;
	int nr_channels = 0;

	ImageID image_id = ImageCodex::INVALID_IMAGE_ID;

	std::visit( fastgltf::visitor{
		[]( auto& arg ) {},

		// ----------
		// File
		[&]( fastgltf::sources::URI& file_path ) {
			assert( false && "Not implemented yet!" );
		},

		// ----------
		// Vector
		[&]( fastgltf::sources::Vector& vector ) {
			assert( false && "Not implemented yet!" );
		},

		// ----------
		// Buffer
		[&]( fastgltf::sources::BufferView& buffer_view ) {
			auto& view = asset.bufferViews[buffer_view.bufferViewIndex];
			auto& buffer = asset.buffers[view.bufferIndex];

			// LoadExternalBuffers was passed on the Loader, so all buffers
			// are loaded as actual vectors
			std::visit( fastgltf::visitor{
				[]( auto& arg ) {},
				[&]( fastgltf::sources::Vector& vector ) {
					std::string name = std::format( "{}.{}", buffer_view.bufferViewIndex, imageMimeToStr( buffer_view.mimeType ) );
					fmt::println( "Loading Image: {}", name.c_str( ) );

					unsigned char* data = stbi_load_from_memory( vector.bytes.data( ) + view.byteOffset, (int)view.byteLength, &width, &height, &nr_channels, 4 );
					if ( data ) {
						VkExtent3D size = {
							.width = (uint32_t)(width),
							.height = (uint32_t)(height),
							.depth = 1
						};

						image_id = gfx.image_codex.loadImageFromData( name, data, size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true );

						stbi_image_free( data );
					} else {
						fmt::println( "Failed to load image {}", image.name.c_str( ) );
					}
				}
			}, buffer.data );
		},
		}, image.data );

	return image_id;
}

static std::vector<ImageID> loadImages( GfxDevice& gfx, fastgltf::Asset& asset ) {
	std::vector<ImageID> images;

	for ( fastgltf::Image& image : asset.images ) {
		images.push_back( loadImage( gfx, asset, image ) );
	}

	return images;
}

static MeshID loadPrimitive( GfxDevice& gfx, fastgltf::Asset& asset, fastgltf::Primitive& primitive ) {
	Mesh mesh;
	mesh.vertices.clear( );
	mesh.indices.clear( );

	// ----------
	// Index
	{
		fastgltf::Accessor& index_accessor = asset.accessors[primitive.indicesAccessor.value( )];
		mesh.indices.reserve( index_accessor.count );

		fastgltf::iterateAccessor<uint32_t>( asset, index_accessor, [&]( uint32_t index ) {
			mesh.indices.push_back( index );
		} );
	}

	// ----------
	// Vertex
	{
		fastgltf::Accessor& vertex_accessor = asset.accessors[primitive.findAttribute( "POSITION" )->second];
		mesh.vertices.resize( vertex_accessor.count );

		fastgltf::iterateAccessorWithIndex<vec3>( asset, vertex_accessor, [&]( vec3 v, size_t index ) {
			Mesh::Vertex vertex = {};
			vertex.position = v;
			mesh.vertices[index] = vertex;
		} );
	}

	// ----------
	// Normals
	{
		auto accessor_index = primitive.findAttribute( "NORMAL" );
		if ( accessor_index != primitive.attributes.end( ) ) {
			fastgltf::iterateAccessorWithIndex<vec3>( asset, asset.accessors[accessor_index->second], [&]( vec3 value, size_t index ) {
				mesh.vertices[index].normal = value;
			} );
		}
	}

	// ----------
	// UVs
	{
		auto accessor_index = primitive.findAttribute( "TEXCOORD_0" );
		if ( accessor_index != primitive.attributes.end( ) ) {
			fastgltf::iterateAccessorWithIndex<vec2>( asset, asset.accessors[accessor_index->second], [&]( vec2 value, size_t index ) {
				mesh.vertices[index].uv_x = value.x;
				mesh.vertices[index].uv_y = value.y;
			} );
		}
	}

	// ----------
	// Tangent
	{
		auto accessor_index = primitive.findAttribute( "TANGENT" );
		if ( accessor_index != primitive.attributes.end( ) ) {
			fastgltf::iterateAccessorWithIndex<vec4>( asset, asset.accessors[accessor_index->second], [&]( vec4 value, size_t index ) {
				mesh.vertices[index].tangent = value;
			} );
		}
	}

	return gfx.mesh_codex.addMesh( gfx, mesh );
}

static std::vector<Scene::MeshAsset> loadMeshes( GfxDevice& gfx, fastgltf::Asset& asset ) {
	std::vector<Scene::MeshAsset> mesh_assets;

	for ( fastgltf::Mesh& mesh : asset.meshes ) {
		Scene::MeshAsset mesh_asset = {};

		fmt::println( "Loading Mesh: {}", mesh.name.c_str( ) );

		auto id = 0u;
		for ( fastgltf::Primitive& primitive : mesh.primitives ) {
			fmt::println( "Loading Primitive: {}", id++ );

			mesh_asset.primitives.push_back( loadPrimitive( gfx, asset, primitive ) );
			mesh_asset.materials.push_back( primitive.materialIndex.value( ) );
		}

		mesh_assets.push_back( mesh_asset );
	}

	return mesh_assets;
}

static MaterialID loadMaterial( GfxDevice& gfx, fastgltf::Asset& asset, fastgltf::Material& gltf_material, const std::vector<ImageID>& image_ids ) {
	Material material = {};
	material.name = gltf_material.name;

	auto color = gltf_material.pbrData.baseColorFactor;
	material.base_color = vec4{ color[0],color[1],color[2],color[3] };

	material.metalness_factor = gltf_material.pbrData.metallicFactor;
	material.roughness_factor = gltf_material.pbrData.roughnessFactor;

	auto imageOrDefault = [&]( fastgltf::Optional<size_t> image_index ) {
		if ( !image_index.has_value( ) ) {
			return gfx.image_codex.getChekboardImageId( );
		}

		return image_ids.at( image_index.value( ) );
	};

	// ----------
	// Albedo Texture
	if ( gltf_material.pbrData.baseColorTexture.has_value( ) ) {
		auto idx = asset.textures[gltf_material.pbrData.baseColorTexture.value( ).textureIndex].imageIndex;
		material.color_id = imageOrDefault( idx );
	}

	// ----------
	// Metallic Roughness Texture
	if ( gltf_material.pbrData.metallicRoughnessTexture.has_value( ) ) {
		auto idx = asset.textures[gltf_material.pbrData.metallicRoughnessTexture.value( ).textureIndex].imageIndex;
		material.metal_roughness_id = imageOrDefault( idx );
	}


	// ----------
	// Normal Texture
	if ( gltf_material.normalTexture.has_value( ) ) {
		auto idx = asset.textures[gltf_material.normalTexture.value( ).textureIndex].imageIndex;
		material.normal_id = imageOrDefault( idx );
	}

	return gfx.material_codex.addMaterial( gfx, material );
}

static std::vector<MaterialID> loadMaterials( GfxDevice& gfx, fastgltf::Asset& asset, const std::vector<ImageID>& image_ids ) {
	std::vector<MaterialID> materials;

	for ( auto& material : asset.materials ) {
		materials.push_back( loadMaterial( gfx, asset, material, image_ids ) );
	}

	return materials;
}

static void loadHierarchy( GfxDevice& gfx, fastgltf::Asset& asset, Scene& scene ) {

	std::vector<std::shared_ptr<Scene::Node>> nodes;

	// ----------
	// Node List
	for ( auto& node : asset.nodes ) {
		std::shared_ptr<Scene::Node> new_node = std::make_shared<Scene::Node>( );

		if ( node.meshIndex.has_value( ) ) {
			new_node->mesh_index = node.meshIndex.value( );
		}

		new_node->name = node.name;
		nodes.push_back( new_node );

		// ----------
		// Transform
		{
			std::visit( fastgltf::visitor{
				[&]( fastgltf::Node::TransformMatrix matrix ) {
					mat4 transform;
					memcpy( &transform, matrix.data( ), sizeof( matrix ) );

					new_node->transform = Transform3D( transform );
				},
				[&]( fastgltf::Node::TRS transform ) {
					new_node->transform.set_position( vec3{transform.translation[0], transform.translation[1], transform.translation[2]} );
					new_node->transform.set_heading( quat{ transform.rotation[1], transform.rotation[1], transform.rotation[2], transform.rotation[3] } );
					new_node->transform.set_scale( vec3{ transform.scale[0], transform.scale[1], transform.scale[2] } );
				},
				}, node.transform );
		}
	}

	// ----------
	// Hierarchy
	for ( size_t i = 0; i < nodes.size( ); i++ ) {
		auto& node = asset.nodes[i];
		auto& scene_node = nodes[i];

		for ( auto& child : node.children ) {
			scene_node->children.push_back( nodes[child] );
			nodes[child]->parent = scene_node;
		}
	}

	// ----------
	// Top Nodes
	for ( auto& node : nodes ) {
		if ( node->parent.lock( ) == nullptr ) {
			scene.top_nodes.push_back( node );
			node->propagateMatrix( );
		}
	}
}

std::unique_ptr<Scene> GltfLoader::load( GfxDevice& gfx, const std::string& path ) {
	auto start = std::chrono::system_clock::now( );

	auto scene_ptr = std::make_unique<Scene>( );
	auto& scene = *scene_ptr.get( );

	scene.name = path;

	constexpr auto gltf_options =
		fastgltf::Options::DontRequireValidAssetMember |
		fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadGLBBuffers |
		fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::LoadExternalImages;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile( path );

	auto res_gltf = loadFromFile( data, path, gltf_options );
	if ( !res_gltf ) {
		return nullptr;
	}
	auto& gltf = res_gltf.value( );

	// ----------
	// Load Images
	fmt::println( "Loading Images..." );
	std::future<std::vector<ImageID>> images = std::async( std::launch::async, loadImages, std::ref( gfx ), std::ref( gltf ) );

	// ----------
	// Load Meshes
	fmt::println( "Loading Geometry..." );
	std::future<std::vector<Scene::MeshAsset>> meshes = std::async( std::launch::async, loadMeshes, std::ref( gfx ), std::ref( gltf ) );

	auto image_ids = images.get( );
	auto mesh_assets = meshes.get( );

	// ----------
	// Load Materials
	fmt::println( "Loading Materials..." );
	auto material_ids = loadMaterials( gfx, gltf, image_ids );

	// ----------
	// Load Scene Hierarchy
	scene.materials = material_ids;
	scene.meshes = mesh_assets;

	loadHierarchy( gfx, gltf, scene );

	{
		auto end = std::chrono::system_clock::now( );
		auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		auto milliseconds = elapsed.count( ) / 1000.f;
		auto seconds = milliseconds / 1000.0f;

		fmt::println( "GLTF 2.0 loading took: {} seconds", seconds );
	}

	return scene_ptr;
}
