#pragma once

#include <graphics/pipelines/pipeline.h>
#include <vector>

class Pipeline;
class GfxDevice;

class PipelineCodex {
public:
	inline static const PipelineID INVALID_PIPELINE_ID = std::numeric_limits<uint32_t>::max( ) - 1;

	void init( GfxDevice& gfx );
	void cleanup( GfxDevice& gfx );

	const std::vector<std::unique_ptr<Pipeline>> getPipelines( ) {
		return pipelines;
	}

	template<typename T>
	PipelineID createPipeline( GfxDevice& gfx ) {
		PipelineID id = pipelines.size( );

		auto pipeline = std::make_unique<T>( );
		pipeline->init( gfx );
		pipeline_id_cache[pipeline->name] = id;
		pipelines.push_back( std::move( pipeline ) );

		return id;
	}

	template<typename T>
	T* getPipeline( PipelineID id ) {
		return dynamic_cast<T*>(pipelines.at( id ).get( ));
	}

	template<typename T>
	T* getPipeline( const std::string& name ) {
		assert( pipeline_id_cache.find( name ) != pipeline_id_cache.end( ) );

		return dynamic_cast<T*>(pipelines.at( pipeline_id_cache[name] ).get( ));
	}

private:
	void initBuiltinPipelines( GfxDevice& gfx );

	std::vector<std::unique_ptr<Pipeline>> pipelines;
	std::unordered_map<std::string, PipelineID> pipeline_id_cache;
};