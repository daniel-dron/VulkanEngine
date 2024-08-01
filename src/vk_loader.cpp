#include <vk_loader.h>

#include <cstdint>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <memory>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include "fmt/core.h"
#include "stb_image.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_types.h"

#define STB_IMAGE_IMPLEMENTATION
#include <format>

#include "stb_image.h"

void LoadedGltf::Draw( const glm::mat4& top_matrix, DrawContext& ctx ) {
	// create renderables from the scenenodes
	for ( const auto& n : top_nodes ) {
		n->Draw( top_matrix, ctx );
	}
}

void LoadedGltf::clear_all( ) {
	const VkDevice dv = creator->gfx->device;

	descriptor_pool.destroy_pools( dv );
	creator->gfx->free( material_data_buffer );

	for ( auto& [k, v] : meshes ) {
		creator->gfx->free( v->mesh_buffers.indexBuffer );
		creator->gfx->free( v->mesh_buffers.vertexBuffer );
	}

	for ( auto& sampler : samplers ) {
		vkDestroySampler( dv, sampler, nullptr );
	}
}

VkFilter
extract_filter( fastgltf::Filter filter ) {
	switch ( filter ) {
		// nearest samplers
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;

		// linear samplers
	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_FILTER_LINEAR;
	}
}

VkSamplerMipmapMode
extract_mipmap_mode( fastgltf::Filter filter ) {
	switch ( filter ) {
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

#include <imgui_impl_vulkan.h>

#include <random>
#include <string>

std::string
generateRandomNumber( ) {
	std::random_device rd;
	std::mt19937 gen( rd( ) );
	std::uniform_int_distribution<> dis(
		1, 10000000 ); // Generate a number between 1 and 100

	int randomNum = dis( gen );
	return std::to_string( randomNum );
}

std::optional<std::shared_ptr<LoadedGltf>>
loadGltf( VulkanEngine* engine, std::string_view filePath ) {
	fmt::println( "Loading GLTF: {}", filePath );

	auto scene = std::make_shared<LoadedGltf>( );
	scene->creator = engine;
	LoadedGltf& file = *scene.get( );

	fastgltf::Parser parser{};

	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile( filePath );

	fastgltf::Asset gltf;
	std::filesystem::path path = filePath;

	auto type = fastgltf::determineGltfFileType( &data );
	if ( type == fastgltf::GltfType::glTF ) {
		auto load = parser.loadGLTF( &data, path.parent_path( ), gltfOptions );
		if ( load ) {
			gltf = std::move( load.get( ) );
		} else {
			std::cerr << "Failed to load glTF: "
				<< fastgltf::to_underlying( load.error( ) ) << std::endl;
		}
	} else if ( type == fastgltf::GltfType::GLB ) {
		auto load = parser.loadBinaryGLTF( &data, path.parent_path( ), gltfOptions );
		if ( load ) {
			gltf = std::move( load.get( ) );
		} else {
			std::cerr << "Failed to load glTF: "
				<< fastgltf::to_underlying( load.error( ) ) << std::endl;
		}
	} else {
		std::cerr << "Failed to determine glTF container" << std::endl;
		return {};
	}

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
	};
	file.descriptor_pool.init(
		engine->gfx->device, static_cast<uint32_t>(gltf.materials.size( )), sizes );

	//
	// load samplers
	//
	for ( fastgltf::Sampler& sampler : gltf.samplers ) {
		VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr };
		sampl.maxLod = VK_LOD_CLAMP_NONE;
		sampl.minLod = 0;

		sampl.magFilter = extract_filter( sampler.magFilter.value_or( fastgltf::Filter::Nearest ) );
		sampl.minFilter = extract_filter( sampler.minFilter.value_or( fastgltf::Filter::Nearest ) );

		sampl.mipmapMode = extract_mipmap_mode(
			sampler.minFilter.value_or( fastgltf::Filter::Nearest ) );

		VkSampler newSampler;
		vkCreateSampler( engine->gfx->device, &sampl, nullptr, &newSampler );

		file.samplers.push_back( newSampler );
	}

	//
	// mesh loading
	//

	// temporal arrays for all the objects to use while creating the GLTF data
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<ImageID> image_ids;
	std::vector<std::shared_ptr<GltfMaterial>> materials;

	// load all textures
	for ( fastgltf::Image& image : gltf.images ) {
		auto loaded_img = loadImage( engine, gltf, image );

		if ( loaded_img.has_value( ) ) {
			image_ids.push_back( (*loaded_img).first );

			auto name = loaded_img.value( ).second;
			if ( name == "no_name" ) {
				name = generateRandomNumber( );
			}

			file.images.push_back( loaded_img.value( ).first );
		} else {
			image_ids.push_back( engine->error_checkerboard_image );
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
	}

	// create buffer to hold all the material data
	file.material_data_buffer = engine->gfx->allocate(
		sizeof( GltfMetallicRoughness::MaterialConstants ) * gltf.materials.size( ),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		"materialDataBuffer" );

	int data_index = 0;
	auto sceneMaterialConstants = (GltfMetallicRoughness::MaterialConstants*)
		file.material_data_buffer.info.pMappedData;

	for ( fastgltf::Material& mat : gltf.materials ) {
		auto newMat = std::make_shared<GltfMaterial>( );
		materials.push_back( newMat );
		file.materials[mat.name.c_str( )] = newMat;
		newMat->name = mat.name;

		fmt::println( "Loading mat: {}", mat.name.c_str( ) );

		GltfMetallicRoughness::MaterialConstants constants;
		constants.color_factors.x = mat.pbrData.baseColorFactor[0];
		constants.color_factors.y = mat.pbrData.baseColorFactor[1];
		constants.color_factors.z = mat.pbrData.baseColorFactor[2];
		constants.color_factors.w = mat.pbrData.baseColorFactor[3];

		constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
		constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
		sceneMaterialConstants[data_index] = constants;

		auto passType = MaterialPass::MainColor;
		if ( mat.alphaMode == fastgltf::AlphaMode::Blend ) {
			passType = MaterialPass::Transparent;
		}

		GltfMetallicRoughness::MaterialResources materialResources;
		materialResources.color_image = engine->white_image;
		materialResources.color_image = engine->white_image;
		materialResources.color_sampler = engine->default_sampler_linear;
		materialResources.metal_rough_image = engine->white_image;
		materialResources.metal_rough_sampler = engine->default_sampler_linear;

		// set the uniform buffer for the material data
		materialResources.data_buffer = file.material_data_buffer.buffer;
		materialResources.data_buffer_offset = data_index * sizeof( GltfMetallicRoughness::MaterialConstants );

		// ----------
		// grab textures
		if ( mat.pbrData.baseColorTexture.has_value( ) ) {
			size_t img = gltf.textures[mat.pbrData.baseColorTexture.value( ).textureIndex]
				.imageIndex.value( );
			size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value( ).textureIndex]
				.samplerIndex.value( );

			materialResources.color_image = image_ids[img];
			materialResources.color_sampler = file.samplers[sampler];

			auto& timg = engine->gfx->image_codex.getImage( image_ids[img] );
			newMat->debug_sets.base_color_set = ImGui_ImplVulkan_AddTexture( file.samplers[sampler],
				timg.view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		}

		if ( mat.pbrData.metallicRoughnessTexture.has_value( ) ) {
			size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value( ).textureIndex]
				.imageIndex.value( );
			size_t sampler = gltf
				.textures[mat.pbrData.metallicRoughnessTexture.value( ).texCoordIndex]
				.samplerIndex.value( );

			materialResources.metal_rough_image = image_ids[img];
			materialResources.metal_rough_sampler = file.samplers[sampler];
			auto& timg = engine->gfx->image_codex.getImage( image_ids[img] );
			newMat->debug_sets.metal_roughness_set = ImGui_ImplVulkan_AddTexture( file.samplers[sampler],
				timg.view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		}

		if ( mat.normalTexture.has_value( ) ) {
			size_t img = gltf.textures[mat.normalTexture.value( ).textureIndex]
				.imageIndex.value( );
			size_t sampler = gltf.textures[mat.normalTexture.value( ).texCoordIndex]
				.samplerIndex.value( );

			materialResources.normal_map = image_ids[img];
			materialResources.normal_sampler = file.samplers[sampler];
			auto& timg = engine->gfx->image_codex.getImage( image_ids[img] );
			newMat->debug_sets.normal_map_set = ImGui_ImplVulkan_AddTexture( file.samplers[sampler],
				timg.view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		}

		newMat->data = engine->metal_rough_material.writeMaterial(
			engine, passType, materialResources, file.descriptor_pool );

		data_index++;
	}

	//
	// load meshes
	//
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for ( fastgltf::Mesh& mesh : gltf.meshes ) {
		auto newmesh = std::make_shared<MeshAsset>( );
		meshes.push_back( newmesh );
		file.meshes[mesh.name.c_str( )] = newmesh;
		newmesh->name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear( );
		vertices.clear( );

		for ( auto&& p : mesh.primitives ) {
			GeoSurface newSurface;
			newSurface.start_index = (uint32_t)indices.size( );
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value( )].count;

			size_t initial_vtx = vertices.size( );

			// load indexes
			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value( )];
				indices.reserve( indices.size( ) + indexaccessor.count );

				fastgltf::iterateAccessor<std::uint32_t>(
					gltf, indexaccessor, [&]( std::uint32_t idx ) {
					indices.push_back( idx + static_cast<uint32_t>(initial_vtx) );
				} );
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute( "POSITION" )->second];
				vertices.resize( vertices.size( ) + posAccessor.count );

				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf, posAccessor, [&]( glm::vec3 v, size_t index ) {
					Vertex newvtx;
					newvtx.position = v;
					newvtx.normal = { 1, 0, 0 };
					newvtx.color = glm::vec4{ 1.f };
					newvtx.uv_x = 0;
					newvtx.uv_y = 0;
					vertices[initial_vtx + index] = newvtx;
				} );
			}

			// load vertex normals
			auto normals = p.findAttribute( "NORMAL" );
			if ( normals != p.attributes.end( ) ) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf,
					gltf.accessors[(*normals).second],
					[&]( glm::vec3 v, size_t index ) {
					vertices[initial_vtx + index].normal = v;
				} );
			}

			// load UVs
			auto uv = p.findAttribute( "TEXCOORD_0" );
			if ( uv != p.attributes.end( ) ) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(
					gltf, gltf.accessors[(*uv).second], [&]( glm::vec2 v, size_t index ) {
					vertices[initial_vtx + index].uv_x = v.x;
					vertices[initial_vtx + index].uv_y = v.y;
				} );
			}

			// load vertex colors
			auto colors = p.findAttribute( "COLOR_0" );
			if ( colors != p.attributes.end( ) ) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					gltf,
					gltf.accessors[(*colors).second],
					[&]( glm::vec4 v, size_t index ) {
					vertices[initial_vtx + index].color = v;
				} );
			}

			auto tangents = p.findAttribute( "TANGENT" );
			if ( tangents != p.attributes.end( ) ) {
				auto accessor = gltf.accessors[(*tangents).second];
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					gltf, accessor, [&]( glm::vec4 t, size_t index ) {
					vertices[initial_vtx + index].tangent = t;
				} );
			}

			if ( p.materialIndex.has_value( ) ) {
				newSurface.material = materials[p.materialIndex.value( )];
			} else {
				newSurface.material = materials[0];
			}

			glm::vec3 minpos = vertices[initial_vtx].position;
			glm::vec3 maxpos = vertices[initial_vtx].position;
			for ( size_t i = initial_vtx; i < vertices.size( ); i++ ) {
				minpos = glm::min( minpos, vertices[i].position );
				maxpos = glm::max( maxpos, vertices[i].position );
			}
			// calculate origin and extents from the min/max, use extent lenght for
			// radius
			newSurface.bounds.origin = (maxpos + minpos) / 2.f;
			newSurface.bounds.extents = (maxpos - minpos) / 2.f;
			newSurface.bounds.sphereRadius = glm::length( newSurface.bounds.extents );

			newmesh->surfaces.push_back( newSurface );
		}

		newmesh->mesh_buffers = engine->uploadMesh( indices, vertices );
	}

	// load all nodes and their meshes
	for ( fastgltf::Node& node : gltf.nodes ) {
		std::shared_ptr<Node> newNode;

		// find if the node has a mesh, and if it does hook it to the mesh pointer
		// and allocate it with the meshnode class
		if ( node.meshIndex.has_value( ) ) {
			newNode = std::make_shared<MeshNode>( );
			dynamic_cast<MeshNode*>(newNode.get( ))->mesh = meshes[*node.meshIndex];
		} else {
			newNode = std::make_shared<Node>( );
		}

		newNode->name = node.name.c_str( );

		nodes.push_back( newNode );
		file.nodes[node.name.c_str( )];

		std::visit(
			fastgltf::visitor{
				[&]( fastgltf::Node::TransformMatrix matrix ) {
					memcpy( &newNode->localTransform, matrix.data( ), sizeof( matrix ) );
				},
				[&]( fastgltf::Node::TRS transform ) {
					glm::vec3 tl( transform.translation[0],
						transform.translation[1],
						transform.translation[2] );
					glm::quat rot( transform.rotation[3],
						transform.rotation[0],
						transform.rotation[1],
						transform.rotation[2] );
					glm::vec3 sc(
						transform.scale[0], transform.scale[1], transform.scale[2] );

					glm::mat4 tm = glm::translate( glm::mat4( 1.f ), tl );
					glm::mat4 rm = glm::toMat4( rot );
					glm::mat4 sm = glm::scale( glm::mat4( 1.f ), sc );

					newNode->localTransform = tm * rm * sm;
				} },
			node.transform );
	}

	// run loop again to setup transform hierarchy
	for ( int i = 0; i < gltf.nodes.size( ); i++ ) {
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];

		for ( auto& c : node.children ) {
			sceneNode->children.push_back( nodes[c] );
			nodes[c]->parent = sceneNode;
		}
	}

	// find the top nodes, with no parents
	for ( auto& node : nodes ) {
		if ( node->parent.lock( ) == nullptr ) {
			file.top_nodes.push_back( node );
			node->refreshTransform( glm::mat4{ 1.f } );
		}
	}
	return scene;
}

std::optional<std::pair<ImageID, std::string>>
loadImage( VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image ) {
	ImageID image_id = -1;
	int width, height, nrChannels;

	std::string name = "no_name";

	const auto mime_to_str = []( fastgltf::MimeType mime ) {
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

	std::visit(
		fastgltf::visitor{
			[]( auto& arg ) {},
			[&]( fastgltf::sources::URI& filePath ) {
				assert( filePath.fileByteOffset == 0 ); // We don't support offsets with stbi.
				assert( filePath.uri.isLocalPath( ) ); // We're only capable of loading
				// local files.

				const std::string path( filePath.uri.path( ).begin( ),
					filePath.uri.path( ).end( ) ); // Thanks C++.
				unsigned char* data = stbi_load( path.c_str( ), &width, &height, &nrChannels, 4 );
				if ( data ) {
					VkExtent3D imagesize;
					imagesize.width = width;
					imagesize.height = height;
					imagesize.depth = 1;

					image_id = engine->gfx->image_codex.loadImageFromFile(
						path, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true );

					name = filePath.uri.path( );

					stbi_image_free( data );
				}
			},
			[&]( fastgltf::sources::Vector& vector ) {
				unsigned char* data = stbi_load_from_memory( vector.bytes.data( ),
					static_cast<int>(vector.bytes.size( )),
					&width,
					&height,
					&nrChannels,
					4 );
				if ( data ) {
					VkExtent3D imagesize;
					imagesize.width = width;
					imagesize.height = height;
					imagesize.depth = 1;

					image_id = engine->gfx->image_codex.loadImageFromData( "hello.test",
						data,
						imagesize,
						VK_FORMAT_R8G8B8A8_UNORM,
						VK_IMAGE_USAGE_SAMPLED_BIT,
						true );

					stbi_image_free( data );
				}
			},
			[&]( fastgltf::sources::BufferView& view ) {
				auto& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];

				std::visit( fastgltf::visitor {
					// We only care about VectorWithMime here, because we
					// specify LoadExternalBuffers, meaning all buffers
					// are already loaded into a vector.
					[]( auto& arg ) {},
					[&]( fastgltf::sources::Vector& vector ) {
						unsigned char* data = stbi_load_from_memory(
							vector.bytes.data( ) + bufferView.byteOffset,
							static_cast<int>(bufferView.byteLength),
							&width,
							&height,
							&nrChannels,
							4 );
						if ( data ) {
							VkExtent3D imagesize;
							imagesize.width = width;
							imagesize.height = height;
							imagesize.depth = 1;

							std::string debug_name = std::format( "{}.{}",
								view.bufferViewIndex,
								mime_to_str( view.mimeType ) );
							image_id = engine->gfx->image_codex.loadImageFromData(
								debug_name.c_str( ),
								data,
								imagesize,
								VK_FORMAT_R8G8B8A8_UNORM,
								VK_IMAGE_USAGE_SAMPLED_BIT,
								true );

							stbi_image_free( data );
						}
					} },
		 buffer.data );
 },
		},
		image.data );

	// if any of the attempts to load the data failed, we havent written the image
	// so handle is null
	if ( image_id == -1 ) {
		return {};
	} else {
		return std::pair<ImageID, std::string> { image_id, name };
	}
}
