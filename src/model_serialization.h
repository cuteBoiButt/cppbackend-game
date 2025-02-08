#pragma once

#include "model.h"
#include "app.h"

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>

#include <vector>

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog);

    [[nodiscard]] model::Dog Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& vel_;
        ar& dir_;
        ar& bag_capacity_;
        ar& bag_;
        ar& score_;
        ar& age_;
        ar& idle_for_;
        ar& is_idle_;
    }

private:
    size_t id_{0};
    std::string name_;
    geom::Point2D pos_;
    geom::Vec2D vel_;
    model::Direction dir_{model::Direction::NORTH};
    size_t bag_capacity_{0};
    std::vector<std::pair<size_t,size_t>> bag_;
    size_t score_{0};
    size_t age_{0};
    size_t idle_for_{0};
    bool is_idle_{true};
};

class LootGeneratorRepr {
public:
    LootGeneratorRepr() = default;

    explicit LootGeneratorRepr(const loot_gen::LootGenerator& loot_gen);

    void Restore(model::GameSession& session) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& time_without_loot_;
    }

private:
    size_t time_without_loot_{0};
};

class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(const model::GameSession& game_session);

    [[nodiscard]] model::GameSession Restore(const app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_;
        ar& dogs_repr_;
        ar& loot_map_repr_;
        ar& loot_gen_repr_;
        ar& loot_id_;
    }

private:
    std::string map_id_;
    std::vector<DogRepr> dogs_repr_;
    std::unordered_map<size_t, std::pair<size_t, geom::Point2D>> loot_map_repr_;
    LootGeneratorRepr loot_gen_repr_;
    size_t loot_id_{0};
};

class GameRepr {
public:
    GameRepr() = default;

    explicit GameRepr(const model::Game& game);

    void Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& sessions_repr_;
    }

private:
    std::vector<GameSessionRepr> sessions_repr_;
};

class PlayerRepr {
public:
    PlayerRepr() = default;

    explicit PlayerRepr(const model::Player& player);

    [[nodiscard]] model::Player Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& session_id_;
        ar& dog_id_;
    }

private:
    std::string session_id_;
    size_t dog_id_{0};
};

class PlayersRepr {
public:
    PlayersRepr() = default;

    explicit PlayersRepr(const app::Players& players);

    void Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& players_repr_;
    }

private:
    std::vector<PlayerRepr> players_repr_;
};

class PlayerTokensRepr {
public:
    PlayerTokensRepr() = default;

    explicit PlayerTokensRepr(const app::PlayerTokens& player_tokens);

    void Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token_to_player_repr_;
    }

private:
    std::vector<std::pair<std::string, size_t>> token_to_player_repr_;
};

class ApplicationRepr {
public:
    ApplicationRepr() = default;

    explicit ApplicationRepr(const app::Application& app);

    void Restore(app::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& game_repr_;
        ar& players_repr_;
        ar& player_tokens_repr_;
    }

private:
    GameRepr game_repr_;
    PlayersRepr players_repr_;
    PlayerTokensRepr player_tokens_repr_;
};

/* Другие классы модели сериализуются и десериализуются похожим образом */

}  // namespace serialization
