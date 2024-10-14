#pragma once

#include <vector>

template<typename T>
class CircularQueue {
public:
    explicit CircularQueue( size_t size ) :
        m_buffer( size ), m_maxSize( size ) {}

    void Push( const T &item ) {
        m_buffer[m_head] = item;

        if ( m_full ) {
            m_tail = ( m_tail + 1 ) % m_maxSize;
        }

        m_head = ( m_head + 1 ) % m_maxSize;
        m_full = m_head == m_tail;
    }

    T &operator[]( size_t index ) {
        return m_buffer[( m_tail + index ) % m_maxSize];
    }

    const T &operator[]( size_t index ) const {
        return m_buffer[( m_tail + index ) % m_maxSize];
    }

    size_t Size( ) const {
        if ( m_full )
            return m_maxSize;

        if ( m_head >= m_tail )
            return m_head - m_tail;

        return m_maxSize + m_head - m_tail;
    }

    std::vector<T> ToVector( ) {
        std::vector<T> vector;
        vector.reserve( Size( ) );

        for ( int i = 0; i < Size( ); i++ ) {
            auto item = m_buffer[( m_tail + i ) % m_maxSize];
            vector.push_back( item );
        }

        return vector;
    }

private:
    std::vector<T> m_buffer;
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_maxSize = 0;
    bool m_full = false;
};
