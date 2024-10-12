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

#include "workers.h"

WorkerPool::WorkerPool(const size_t numThreads ) {
	for ( size_t i = 0; i < numThreads; i++ ) {
		m_threads.emplace_back( [this] {
			while ( true ) {
				std::unique_ptr<std::function<void( )>> task;

				{
					std::unique_lock<std::mutex> lock( m_mutex );
					m_cv.wait( lock, [this] {
						return !m_tasks.empty( ) || m_stop.load( );
					} );

					if ( m_stop.load( ) && m_tasks.empty( ) ) {
						return;
					}

					if ( !m_tasks.empty( ) ) {
						task = std::move( m_tasks.front( ) );
						m_tasks.pop( );
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
	m_stop.store( true );
	m_cv.notify_all( );

	for ( auto& thread : m_threads ) {
		if ( thread.joinable( ) ) {
			thread.join( );
		}
	}
}

void WorkerPool::Work( std::function<void( )> task ) {
	std::unique_lock<std::mutex> lock( m_mutex );
	m_tasks.emplace( std::make_unique<std::function<void( )>>( std::move( task ) ) );
	m_cv.notify_one( );
}