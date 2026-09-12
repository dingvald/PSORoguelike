#include "Engine/World/Pathfinder.h"

#include "Engine/World/Grid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <queue>

namespace psr {

namespace {

    constexpr std::array<Vec2, 8> kNeighborOffsets{{
        {1, 0},
        {1, 1},
        {0, 1},
        {-1, 1},
        {-1, 0},
        {-1, -1},
        {0, -1},
        {1, -1},
    }};

    struct OpenEntry
    {
        int f_score;
        std::size_t index;

        friend bool operator>(const OpenEntry& a, const OpenEntry& b) { return a.f_score > b.f_score; }
    };

    std::vector<Vec2> ReconstructPath(const std::vector<Vec2>& came_from, const std::vector<bool>& has_came_from,
                                      std::size_t width, Vec2 start, Vec2 goal)
    {
        std::vector<Vec2> path;
        Vec2 current = goal;
        while (current != start)
        {
            path.push_back(current);
            const std::size_t index = static_cast<std::size_t>(current.y) * width + static_cast<std::size_t>(current.x);
            if (!has_came_from[index])
                break;
            current = came_from[index];
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

} // namespace

std::vector<Vec2> FindPath(const Grid& grid, Vec2 start, Vec2 goal, const std::function<bool(Vec2)>& is_walkable)
{
    if (start == goal || !grid.Contains(start) || !grid.Contains(goal))
        return {};

    const std::size_t width = static_cast<std::size_t>(grid.GetWidth());
    const std::size_t height = static_cast<std::size_t>(grid.GetHeight());
    const std::size_t cell_count = width * height;
    const auto index_of = [width](Vec2 tile)
    { return static_cast<std::size_t>(tile.y) * width + static_cast<std::size_t>(tile.x); };

    std::vector<int> g_score(cell_count, std::numeric_limits<int>::max());
    std::vector<Vec2> came_from(cell_count);
    std::vector<bool> has_came_from(cell_count, false);
    std::vector<bool> closed(cell_count, false);

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<>> open;

    g_score[index_of(start)] = 0;
    open.push({ChebyshevDistance(start, goal), index_of(start)});

    while (!open.empty())
    {
        const std::size_t current_index = open.top().index;
        open.pop();

        if (closed[current_index])
            continue;
        closed[current_index] = true;

        const Vec2 current{static_cast<int>(current_index % width), static_cast<int>(current_index / width)};
        if (current == goal)
            return ReconstructPath(came_from, has_came_from, width, start, goal);

        for (Vec2 offset : kNeighborOffsets)
        {
            const Vec2 neighbor = current + offset;
            if (!grid.Contains(neighbor))
                continue;
            const std::size_t neighbor_index = index_of(neighbor);
            if (closed[neighbor_index])
                continue;
            if (!is_walkable(neighbor))
                continue;

            const int tentative_g_score = g_score[current_index] + 1;
            if (tentative_g_score < g_score[neighbor_index])
            {
                g_score[neighbor_index] = tentative_g_score;
                came_from[neighbor_index] = current;
                has_came_from[neighbor_index] = true;
                open.push({tentative_g_score + ChebyshevDistance(neighbor, goal), neighbor_index});
            }
        }
    }

    return {};
}

} // namespace psr
