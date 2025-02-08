#pragma once

#include "tagged.h"
#include "tagged_uuid.h"
#include "extra_data.h"
#include "loot_generator.h"
#include "geom.h"

#include <optional>

namespace model {

enum Direction {
    NORTH = 0,
    SOUTH,
    WEST,
    EAST
};

constexpr std::array<std::string_view, 4> DIR_TO_STRING = {"U", "D", "L", "R"};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, geom::Point start, geom::Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, geom::Point start, geom::Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    geom::Point GetStart() const noexcept {
        return start_;
    }

    geom::Point GetEnd() const noexcept {
        return end_;
    }

private:
    geom::Point start_;
    geom::Point end_;
};

class Building {
public:
    explicit Building(geom::Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const geom::Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    geom::Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, geom::Point position, geom::Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    geom::Point GetPosition() const noexcept {
        return position_;
    }

    geom::Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    geom::Point position_;
    geom::Offset offset_;
};

class RoadGrid {
public:
    using Grid = std::unordered_map<geom::Point, bool, geom::PointHasher>;

    void AddRoad(const Road* road);
    bool ContainsRoad(const geom::Point& p) const;
private:
    Grid grid_;
};

class Dog {
public:
    Dog(std::string_view name, geom::Point2D pos, geom::Vec2D vel = {}, size_t bag_capacity = 3, size_t id = id_counter_++)
        : id_{id}
        , name_{name}
        , pos_{pos}
        , vel_{vel}
        , bag_capacity_{bag_capacity} {}

    std::string_view GetName() const {
        return name_;
    }

    auto GetId() const noexcept {
        return id_;
    }

    const auto& GetPos() const noexcept {
        return pos_;
    }

    const auto& GetVelocity() const noexcept {
        return vel_;
    }

    void SetPos(const geom::Point2D& pos) {
        pos_ = pos;
    }

    void SetVelocity(const geom::Vec2D& vel) {
        vel_ = vel;
    }

    auto GetDir() const noexcept {
        return dir_;
    }

    void SetDir(Direction dir) {
        dir_ = dir;
    }

    bool TryGrabItem(size_t id, size_t type);

    auto& GetBag() const {
        return bag_;
    }

    void ClearBag() {
        bag_.clear();
    }

    auto GetScore() const noexcept {
        return score_;
    }

    void SetScore(size_t score) {
        score_ = score;
    }

    auto GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    auto GetAge() const noexcept {
        return age_;
    }

    void SetAge(std::chrono::milliseconds age) {
        age_ = age;
    }

    auto GetIdleFor() const noexcept {
        return idle_for_;
    }

    void SetIdleFor(std::chrono::milliseconds idle_for) {
        idle_for_ = idle_for;
    }

    auto IsIdle() const noexcept {
        return is_idle_;
    }

    void SetIdle(bool idle = true) {
        is_idle_ = idle;
    }

private:
    size_t id_;
    std::string name_;
    geom::Point2D pos_;
    geom::Vec2D vel_;
    Direction dir_{Direction::NORTH};

    const size_t bag_capacity_;
    std::vector<std::pair<size_t,size_t>> bag_;

    size_t score_{0};

    std::chrono::milliseconds age_{0};
    std::chrono::milliseconds idle_for_{0};

    bool is_idle_{true};

    static size_t id_counter_;
};

using DogPtr = std::shared_ptr<Dog>;
using ConstDogPtr = std::shared_ptr<const Dog>;

class RetiredDog {
public:
    using Id = util::TaggedUUID<RetiredDog>;
    RetiredDog(Id id, std::string name, int score, int play_time_ms)
        : id_(std::move(id))
        , name_(std::move(name))
        , score_{score}
        , play_time_ms_{play_time_ms} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    int GetScore() const noexcept {
        return score_;
    }

    int GetPlayTime() const noexcept {
        return play_time_ms_;
    }

private:
    Id id_;
    std::string name_;
    int score_{0};
    int play_time_ms_{0};
};

class RetiredDogRepository {
public:
    virtual void Save(const RetiredDog& retired_dog) = 0;
    virtual std::vector<RetiredDog> FetchRange(int offset, int size) = 0;
protected:
    virtual ~RetiredDogRepository() = default;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<std::unique_ptr<Road>>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, double dog_speed, const ExtraData& extra_data, size_t bag_capacity) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , dog_speed_{dog_speed}
        , extra_data_{extra_data}
        , bag_capacity_{bag_capacity} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    auto GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(std::unique_ptr<Road> road);

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    const RoadGrid& GetRoadGrid() const noexcept {
        return road_grid_;
    }

    const ExtraData& GetExtraData() const noexcept {
        return extra_data_;
    }

    ExtraData& GetExtraData() {
        return extra_data_;
    }

    size_t GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void MoveDog(Dog* dog, std::chrono::milliseconds delta_ms) const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_;

    RoadGrid road_grid_;

    ExtraData extra_data_;

    size_t bag_capacity_;
};

class GameSession {
public:
    GameSession() = delete;

    explicit GameSession(const Map* map, std::chrono::milliseconds period, double prob)
                         : map_{map}
                         , loot_gen_{period, prob} {}

    const DogPtr& CreateDog(std::string_view name, geom::Point2D pos);

    const auto& GetDogs() const noexcept {
        return dogs_;
    }

    auto& GetDogs() {
        return dogs_;
    }

    void AddDog(const DogPtr& dog) {
        dogs_.emplace(dog->GetId(), dog);
    }

    bool RemoveDog(size_t dog_id) {
        return dogs_.erase(dog_id) != 0;
    }

    std::optional<std::reference_wrapper<const DogPtr>> GetDogById(size_t id) const;

    const Map* GetMap() const noexcept {
        return map_;
    }

    auto& GetLoot() {
        return loot_map_;
    }

    auto& GetLoot() const {
        return loot_map_;
    }

    auto& GetLootGenerator() const noexcept {
        return loot_gen_;
    }
    auto& GetLootGenerator() {
        return loot_gen_;
    }

    void AddLoot(std::pair<size_t, geom::Point2D> loot, size_t id = 0) {
        loot_map_.emplace(id == 0 ? ++loot_id_: id, std::move(loot));
    }

    void RemoveLoot(size_t idx) {
        loot_map_.erase(idx);
    }

    size_t GenerateLoot(std::chrono::milliseconds dt) {
        return loot_gen_.Generate(dt, loot_map_.size(), dogs_.size());
    }

    auto GetNextLootId() const noexcept {
        return loot_id_;
    }

    void SetNextLootId(size_t id) {
        loot_id_ = id;
    }

private:
    const Map* map_;
    std::unordered_map<size_t, DogPtr> dogs_;
    std::unordered_map<size_t, std::pair<size_t, geom::Point2D>> loot_map_;
    loot_gen::LootGenerator loot_gen_;
    size_t loot_id_{0};
};

using GameSessionPtr = std::shared_ptr<GameSession>;
using ConstGameSessionPtr = std::shared_ptr<const GameSession>;

class Player {
public:
    Player(const GameSessionPtr& session, const DogPtr& dog)
                                    : session_{session}
                                    , dog_{dog} {}

    auto GetId() const noexcept {
        return dog_->GetId();
    }

    auto GetName() const {
        return dog_->GetName();
    }

    auto& GetSession() const {
        return session_;
    }

    auto& GetDog() const {
        return dog_;
    }
private:
    GameSessionPtr session_;
    DogPtr dog_;
};

using PlayerPtr = std::shared_ptr<Player>;
using ConstPlayerPtr = std::shared_ptr<const Player>;

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept;

    std::optional<std::reference_wrapper<const GameSessionPtr>> FindSession(const Map::Id& id) const noexcept;

    std::optional<std::reference_wrapper<const GameSessionPtr>> GetSession(const Map::Id& id);

    void AddSession(const GameSessionPtr& session) {
        map_id_to_session_.emplace(session->GetMap()->GetId(), session);
    }

    const auto& GetSessions() const noexcept {
        return map_id_to_session_;
    }

    void SetDefaultDogSpeed(double speed) {
        default_dog_speed_ = speed;
    }

    auto GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_;
    }

    void SetLootGenInterval(std::chrono::milliseconds interval) {
        loot_gen_interval_ = interval;
    }

    auto GetLootGenInterval() const noexcept {
        return loot_gen_interval_;
    }

    void SetLootGenProbability(double prob) {
        loot_gen_prob_ = prob;
    }

    auto GetLootGenProbability() const noexcept {
        return loot_gen_prob_;
    }

    auto GetDefaultBagCapacity() const noexcept {
        return default_bag_capacity_;
    }

    void SetDefaultBagCapacity(size_t default_bag_capacity) {
        default_bag_capacity_ = default_bag_capacity;
    }

    auto GetMaxIdleTime() const noexcept {
        return max_idle_time_;
    }

    void SetMaxIdleTime(std::chrono::milliseconds max_idle_time) {
        max_idle_time_ = max_idle_time;
    }
private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    using MapIdToSession = std::unordered_map<Map::Id, GameSessionPtr, MapIdHasher>;
    MapIdToSession map_id_to_session_;

    double default_dog_speed_{1.0};

    std::chrono::milliseconds loot_gen_interval_{0};
    double loot_gen_prob_{0.0};

    size_t default_bag_capacity_{3};

    std::chrono::milliseconds max_idle_time_{60000};
};

}  // namespace model
