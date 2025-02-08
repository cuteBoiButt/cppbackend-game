#pragma once

#include "collision_detector.h"

#include <boost/json.hpp>

#include <vector>

class ExtraData {
public:
    explicit ExtraData(boost::json::array loot);

    const auto& GetLootTypes() const noexcept {
        return loot_types_;
    }

    const auto& GetBases() const noexcept {
        return bases_;
    }

    void AddBase(collision_detector::Item item);

private:
    boost::json::array loot_types_;
    std::vector<collision_detector::Item> bases_;
};