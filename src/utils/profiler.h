#pragma once
#include "circular_buffer.h"
#include "imgui.h"

namespace utils {

    // https://github.com/Raikiri/LegitProfiler.git
    namespace colors {
        // https://flatuicolors.com/palette/defo
#define RGBA_LE( col ) ( ( ( col & 0xff000000 ) >> ( 3 * 8 ) ) + ( ( ( col ) & 0x00ff0000 ) >> ( 1 * 8 ) ) + ( ( ( col ) & 0x0000ff00 ) << ( 1 * 8 ) ) + ( ( ( col ) & 0x000000ff ) << ( 3 * 8 ) ) )
        static constexpr uint32_t TURQUOISE = RGBA_LE( 0x1abc9cffu );
        static constexpr uint32_t GREEN_SEA = RGBA_LE( 0x16a085ffu );

        static constexpr uint32_t EMERALD = RGBA_LE( 0x2ecc71ffu );
        static constexpr uint32_t NEPHRITIS = RGBA_LE( 0x27ae60ffu );

        static constexpr uint32_t PETER_RIVER = RGBA_LE( 0x3498dbffu );
        static constexpr uint32_t BELIZE_HOLE = RGBA_LE( 0x2980b9ffu );

        static constexpr uint32_t AMETHYST = RGBA_LE( 0x9b59b6ffu );
        static constexpr uint32_t WISTERIA = RGBA_LE( 0x8e44adffu );

        static constexpr uint32_t SUN_FLOWER = RGBA_LE( 0xf1c40fffu );
        static constexpr uint32_t ORANGE = RGBA_LE( 0xf39c12ffu );

        static constexpr uint32_t CARROT = RGBA_LE( 0xe67e22ffu );
        static constexpr uint32_t PUMPKIN = RGBA_LE( 0xd35400ffu );

        static constexpr uint32_t ALIZARIN = RGBA_LE( 0xe74c3cffu );
        static constexpr uint32_t POMEGRANATE = RGBA_LE( 0xc0392bffu );

        static constexpr uint32_t CLOUDS = RGBA_LE( 0xecf0f1ffu );
        static constexpr uint32_t SILVER = RGBA_LE( 0xbdc3c7ffu );

        static constexpr uint32_t IMGUI_TEXT = RGBA_LE( 0xF2F5FAFFu );
        static constexpr uint32_t BACKGROUND = RGBA_LE( 0x2626267Cu );
    } // namespace colors

    struct FrameDataAggregator {
        CircularQueue<double> timers = CircularQueue<double>( 10 );
        std::string name;
        uint32_t color;

        // updated during render
        double average;

        void Push( double time ) {
            timers.Push( time );
        }
    };

    class VisualProfiler {
    public:
        enum TaskType {
            Cpu,
            Gpu,
        };

        explicit VisualProfiler( size_t maxFrames );

        void RegisterTask( const std::string &taskName, uint32_t color, TaskType taskType );
        void AddTimer( const std::string &taskName, double time, TaskType taskType );

        void Render( ImVec2 position, ImVec2 size );

    private:
        std::unordered_map<std::string, FrameDataAggregator> m_frameDataMapCpu;
        std::unordered_map<std::string, FrameDataAggregator> m_frameDataMapGpu;

        size_t m_maxFrames = 0;
        size_t m_currentFrame = 0;
    };


    inline double GetTime( ) {
        static auto start_time = std::chrono::high_resolution_clock::now( );
        const auto now = std::chrono::high_resolution_clock::now( );
        return std::chrono::duration<double>( now - start_time ).count( );
    }

} // namespace utils
