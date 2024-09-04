#include "pipeline_codex.h"

void PipelineCodex::init( GfxDevice& gfx ) {

	initBuiltinPipelines( gfx );
}

void PipelineCodex::cleanup(GfxDevice& gfx ) {
	for ( auto& pipeline : pipelines ) {
		pipeline->cleanup( gfx );
	}

	pipelines.clear( );
	pipeline_id_cache.clear( );
}

void PipelineCodex::initBuiltinPipelines( GfxDevice& gfx ) {
	
}
