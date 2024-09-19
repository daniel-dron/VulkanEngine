#include "workers.h"

WorkerPool::WorkerPool( size_t num_threads ) {
	for ( size_t i = 0; i < num_threads; i++ ) {
		threads.emplace_back( [this] {
			while ( true ) {
				std::unique_ptr<std::function<void( )>> task;

				{
					std::unique_lock<std::mutex> lock( mutex );
					cv.wait( lock, [this] {
						return !tasks.empty( ) || stop.load( );
					} );

					if ( stop.load( ) && tasks.empty( ) ) {
						return;
					}

					if ( !tasks.empty( ) ) {
						task = std::move( tasks.front( ) );
						tasks.pop( );
					}
				}

				if ( task ) {
					(*task)();
				}
			}
		} );
	}
}

WorkerPool::~WorkerPool( ) {
	stop.store( true );
	cv.notify_all( );

	for ( auto& thread : threads ) {
		if ( thread.joinable( ) ) {
			thread.join( );
		}
	}
}

void WorkerPool::work( std::function<void( )> task ) {
	std::unique_lock<std::mutex> lock( mutex );
	tasks.emplace( std::make_unique<std::function<void( )>>( std::move( task ) ) );
	cv.notify_one( );
}