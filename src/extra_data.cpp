#include "extra_data.h"

ExtraData::ExtraData(boost::json::array loot) : loot_types_(std::move(loot)) {}

void ExtraData::AddBase(collision_detector::Item item) {
    bases_.emplace_back(std::move(item));
}
