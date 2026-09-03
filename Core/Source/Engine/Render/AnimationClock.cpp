#include "Engine/Render/AnimationClock.h"

namespace psr {

void AnimationClock::Update(float delta_time)
{
    for (auto& [frame_time, bucket] : m_clocks)
    {
        bucket.elapsed += delta_time;
        while (bucket.elapsed >= frame_time)
        {
            bucket.elapsed -= frame_time;
            ++bucket.tick;
        }
    }
}

int AnimationClock::GetFrameIndex(float frame_time, int frame_count) const
{
    if (frame_count <= 1 || frame_time <= 0.0f)
        return 0;

    return static_cast<int>(m_clocks[frame_time].tick % static_cast<std::uint64_t>(frame_count));
}

} // namespace psr
