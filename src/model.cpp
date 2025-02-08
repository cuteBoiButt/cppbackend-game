#include "model.h"

#include <stdexcept>
#include <cmath>

namespace model {
using namespace std::literals;

size_t Dog::id_counter_{0};

const DogPtr& GameSession::CreateDog(std::string_view name, geom::Point2D pos) {
    auto dog = std::make_shared<Dog>(name, pos, geom::Vec2D{}, map_->GetBagCapacity());
    return dogs_.emplace(dog->GetId(), std::move(dog)).first->second;
}

std::optional<std::reference_wrapper<const DogPtr>> GameSession::GetDogById(size_t id) const {
    if (auto it = dogs_.find(id); it != dogs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Dog::TryGrabItem(size_t id, size_t type) {
    if(bag_.size() >= bag_capacity_) return false;
    bag_.emplace_back(id, type);
    return true;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
        extra_data_.AddBase({{static_cast<double>(o.GetPosition().x), static_cast<double>(o.GetPosition().y)}, 0.5});
    } catch (std::exception&) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::AddRoad(std::unique_ptr<Road> road) {
    roads_.emplace_back(std::move(road));
    road_grid_.AddRoad(roads_.back().get());
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (std::exception&) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

std::optional<std::reference_wrapper<const GameSessionPtr>> Game::FindSession(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_session_.find(id); it != map_id_to_session_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void RoadGrid::AddRoad(const Road* road) {
    if (road->IsHorizontal()) {
        auto y = road->GetStart().y;
        for (auto x = std::min(road->GetStart().x, road->GetEnd().x); x <= std::max(road->GetStart().x, road->GetEnd().x); ++x) {
            grid_[{x, y}] = true;
        }
    } else {
        auto x = road->GetStart().x;
        for (auto y = std::min(road->GetStart().y, road->GetEnd().y); y <= std::max(road->GetStart().y, road->GetEnd().y); ++y) {
            grid_[{x, y}] = true;
        }
    }
}

bool RoadGrid::ContainsRoad(const geom::Point& p) const {
    return grid_.find(p) != grid_.end();
}

void Map::MoveDog(Dog* dog, std::chrono::milliseconds delta_ms) const {
    static constexpr double allowance = 0.4;
    static constexpr double eps = 1e-6;
    static constexpr double millis_per_second = 1000.0;

    auto dt = delta_ms.count() / millis_per_second;
    auto currentPos = dog->GetPos();
    auto velocity = dog->GetVelocity();

    auto cell_x = static_cast<geom::Coord>(std::round(currentPos.x));
    auto cell_y = static_cast<geom::Coord>(std::round(currentPos.y));

    const auto is_on_vertical = road_grid_.ContainsRoad({cell_x, cell_y + 1}) || road_grid_.ContainsRoad({cell_x, cell_y - 1});
    const auto is_on_horizontal = road_grid_.ContainsRoad({cell_x + 1, cell_y}) || road_grid_.ContainsRoad({cell_x - 1, cell_y});

    const auto y_offset_out_of_range = std::abs(currentPos.y - cell_y) > allowance + eps;
    const auto x_offset_out_of_range = std::abs(currentPos.x - cell_x) > allowance + eps;

    auto move_axis = [this, dog, dt](double& pos, double vel, geom::Coord& cell, bool is_on_axis, bool offset_out_of_range, geom::Coord fixed_coord, bool is_x_axis) {
        if (vel) {
            double d = vel * dt;
            auto target = pos + d;
            auto target_cell = static_cast<geom::Coord>(std::round(target));
            const geom::Coord step = (d > 0.0) ? 1 : -1;

            const auto cant_move_along_axis = offset_out_of_range && is_on_axis;

            if (!cant_move_along_axis) {
                while (cell != target_cell) {
                    if (road_grid_.ContainsRoad({is_x_axis ? cell + step : fixed_coord, is_x_axis ? fixed_coord : cell + step})) {
                        cell += step;
                    } else {
                        break;
                    }
                }
            }

            double curr = cell;
            auto diff = target - curr;
            const geom::Coord diff_step = (diff > 0.0) ? 1 : -1;

            const auto is_road_ahead = road_grid_.ContainsRoad({is_x_axis ? cell + diff_step : fixed_coord, is_x_axis ? fixed_coord : cell + diff_step});

            if (step == diff_step && (cant_move_along_axis || !is_road_ahead) && std::abs(diff) > allowance) {
                dog->SetIdle(true);
                dog->SetVelocity({0, 0});
                diff = std::clamp(diff, -allowance, allowance);
            }

            pos = curr + diff;
        }
    };

    move_axis(currentPos.x, velocity.x, cell_x, is_on_vertical, y_offset_out_of_range, cell_y, true);
    move_axis(currentPos.y, velocity.y, cell_y, is_on_horizontal, x_offset_out_of_range, cell_x, false);
    dog->SetPos(currentPos);
}

std::optional<std::reference_wrapper<const GameSessionPtr>> Game::GetSession(const Map::Id& id) {
    auto map = FindMap(id);
    if (map == nullptr) {
        return std::nullopt;
    }

    if (!map_id_to_session_.count(id)) {
        map_id_to_session_.emplace(std::make_pair(id, std::make_unique<GameSession>(map, loot_gen_interval_, loot_gen_prob_)));
    }
    return std::cref(map_id_to_session_.at(id));
}

}  // namespace model
