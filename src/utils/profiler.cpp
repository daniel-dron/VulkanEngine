#include <pch.h>

#include "profiler.h"

#include <imgui.h>

utils::VisualProfiler::VisualProfiler( size_t maxFrames ) : m_maxFrames( maxFrames ) {}
void utils::VisualProfiler::RegisterTask( const std::string &taskName, uint32_t color, TaskType taskType ) {
    const FrameDataAggregator frame_data_aggregator = {
            .timers = CircularQueue<double>( m_maxFrames ),
            .name = taskName,
            .color = color,
    };

    if ( taskType == Cpu ) {
        m_frameDataMapCpu[taskName] = frame_data_aggregator;
    }
    else {
        m_frameDataMapGpu[taskName] = frame_data_aggregator;
    }
}

void utils::VisualProfiler::AddTimer( const std::string &taskName, double time, TaskType taskType ) {
    std::unordered_map<std::string, FrameDataAggregator> *aggregator;

    if ( taskType == Cpu ) {
        aggregator = &m_frameDataMapCpu;
    }
    else {
        aggregator = &m_frameDataMapGpu;
    }

    if ( aggregator->contains( taskName ) ) {
        aggregator->at( taskName ).Push( time );
    }
    else {
        FrameDataAggregator frame_data_aggregator = {
                .timers = CircularQueue<double>( m_maxFrames ),
                .name = taskName,
                .color = colors::AMETHYST,
        };
        frame_data_aggregator.Push( time );
        ( *aggregator )[taskName] = frame_data_aggregator;
    }
}

// this is implemented in a naive way, but its performance impact is minimal on its current used scale
void utils::VisualProfiler::Render( ImVec2 position, ImVec2 size ) {
    ImDrawList *draw_list = ImGui::GetForegroundDrawList( );

    // max bar timer is 30fps (33ms)
    constexpr auto max_time = 1.0f / 30.0f;
    constexpr int bar_width = 5; // 5px
    constexpr int bar_pad = 1; // 1px
    const int bars_amount = static_cast<int>( size.x ) / ( bar_width + bar_pad );

    // calculate frame boundries
    // 1 for CPU graph and another for GPU graph
    constexpr int vertical_padding = 10; // 10 px
    const f32 graph_height = size.y / 2 - vertical_padding;

    auto draw_graph = [&]( const ImVec2 &topLeft, const ImVec2 &bottomRight,
                           std::unordered_map<std::string, FrameDataAggregator> &frameData ) {
        draw_list->AddRectFilled( topLeft, bottomRight, colors::BACKGROUND );

        for ( int i = 0; i < bars_amount; i++ ) {
            f32 previous_bar_top = 0;

            for ( auto &aggregator : frameData | std::views::values ) {
                const u64 index = aggregator.timers.Size( ) - i - 1;

                const auto time = aggregator.timers[index];
                f32 bar_height = ( f32 )( graph_height * ( time / max_time ) );

                if ( bar_height > graph_height ) {
                    bar_height = graph_height;
                }

                int position_x_offset = ( i + 1 ) * bar_width + i * bar_pad;
                float position_x = topLeft.x + size.x - position_x_offset - bar_pad;

                draw_list->AddRectFilled( { position_x, bottomRight.y - bar_height - previous_bar_top },
                                          { position_x + bar_width, bottomRight.y - previous_bar_top },
                                          aggregator.color, 5.0f );

                previous_bar_top += bar_height;

                // contribute to average
                aggregator.average += time;
            }
        }

        // text
        {
            auto pos = ImVec2{ bottomRight.x, topLeft.y };

            for ( auto &aggregator : frameData | std::views::values ) {
                auto text = fmt::format( "{}: {:.3f}ms", aggregator.name.c_str( ),
                                         aggregator.average / static_cast<float>( bars_amount ) * 1000.0f );
                draw_list->AddText( pos, aggregator.color, text.c_str( ) );
                pos.y += 20;
                aggregator.average = 0.0f;
            }
        }
    };

    // cpu
    {
        const ImVec2 top_left = { position.x, position.y + size.y - graph_height };
        const ImVec2 bottom_right = { position.x + size.x, position.y + size.y };
        draw_graph( top_left, bottom_right, m_frameDataMapCpu );
    }

    // gpu
    {
        const ImVec2 top_left = { position.x, position.y };
        const ImVec2 bottom_right = { position.x + size.x, position.y + graph_height };
        draw_graph( top_left, bottom_right, m_frameDataMapGpu );
    }
}
