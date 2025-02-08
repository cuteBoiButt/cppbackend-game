#pragma once

#include "model.h"
#include "db.h"

#include <random>
#include <filesystem>

namespace app {

namespace detail {
static std::random_device RANDOM_DEVICE;
}

class Players {
public:
    const model::PlayerPtr& Add(const model::DogPtr& dog, const model::GameSessionPtr& session);

    auto& GetPlayers() const noexcept {
        return players_;
    }
    auto& GetPlayers() {
        return players_;
    }

    std::optional<std::reference_wrapper<const model::PlayerPtr>> FindPlayerById(size_t id) const;
    bool RemovePlayer(size_t id);

private:
    std::unordered_map<size_t, model::PlayerPtr> players_;
};

class PlayerTokens {
public:
    std::optional<std::reference_wrapper<const model::PlayerPtr>> FindPlayerByToken(const model::Token& token) const noexcept;
    model::Token AddPlayer(const model::PlayerPtr& player);

    auto& GetTokens() const noexcept {
        return token_to_player_;
    }
    auto& GetTokens() {
        return token_to_player_;
    }

    void RemoveToken(size_t player_id);
    void AddTokenForPlayer(const model::Token& token, const model::PlayerPtr& player);

private:
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(detail::RANDOM_DEVICE);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(detail::RANDOM_DEVICE);
    }()};

    std::string GenerateToken();

    using TokenHasher = util::TaggedHasher<model::Token>;
    using TokenToPlayer = std::unordered_map<model::Token, model::PlayerPtr, TokenHasher>;

    TokenToPlayer token_to_player_;
    std::unordered_map<size_t, model::Token> player_id_to_token_;
};

class ApplicationListener {
public:
    virtual ~ApplicationListener() = default;
    virtual void OnTick(std::chrono::milliseconds delta) = 0;
};

class Application {
public:
    Application(const std::filesystem::path& json_path, bool randomize_spawns, std::unique_ptr<db::Database, void(*)(db::Database*)> db);

    const model::Game::Maps& ListMaps() const noexcept;
    const model::Map* FindMap(const model::Map::Id& id) const noexcept;
    std::pair<const model::PlayerPtr&, model::Token> JoinGame(const model::Map::Id& map_id, std::string_view user_name);
    std::optional<std::reference_wrapper<const model::PlayerPtr>> FindPlayerByToken(const model::Token& token);
    
    static void SetPlayerAction(model::Player* player, std::string_view action);
    void Tick(std::chrono::milliseconds dt);
    geom::Point2D GetRandomPointOnMap(const model::Map* map);

    auto& GetGame() const noexcept {
        return game_;
    }
    auto& GetGame() {
        return game_;
    }
    auto& GetTokens() const noexcept {
        return tokens_;
    }
    auto& GetTokens() {
        return tokens_;
    }
    auto& GetPlayers() const noexcept {
        return players_;
    }
    auto& GetPlayers() {
        return players_;
    }

    void AddListener(const std::shared_ptr<ApplicationListener>& listener);
    std::unique_ptr<db::UnitOfWork, void(*)(db::UnitOfWork*)> GetUoW();

private:
    model::Game game_;
    Players players_;
    PlayerTokens tokens_;
    std::minstd_rand rand_{detail::RANDOM_DEVICE()};
    bool random_spawns_;
    std::vector<std::weak_ptr<ApplicationListener>> listeners_;
    std::unique_ptr<db::Database, void(*)(db::Database*)> database_;
};

}
