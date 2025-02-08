#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

bool CollectionResult::IsCollected(double collect_radius) const {
    return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
}

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult{sq_distance, proj_ratio};
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> detected_events;

    for (size_t g = 0; g < provider.GatherersCount(); ++g) {
        const auto& gatherer = provider.GetGatherer(g);
        auto start_pos = gatherer.start_pos;
        auto end_pos = gatherer.end_pos;
        if (start_pos.x == end_pos.x && start_pos.y == end_pos.y) {
            continue; // Gatherer doesn't move, skip
        }

        for (size_t i = 0; i < provider.ItemsCount(); ++i) {
            const auto& item = provider.GetItem(i);
            auto item_pos = item.position;
            auto collect_result = TryCollectPoint(start_pos, end_pos, item_pos);

            double collect_radius = gatherer.width + item.width;
            if (collect_result.IsCollected(collect_radius)) {
                detected_events.emplace_back(i, g, collect_result.sq_distance, collect_result.proj_ratio);
            }
        }
    }

    // Sort events by time
    std::sort(detected_events.begin(), detected_events.end(),
              [](const auto& e1, const auto& e2) {
                  return e1.time < e2.time;
              });

    return detected_events;
}

}  // namespace collision_detector
