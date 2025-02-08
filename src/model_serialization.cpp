#include "model_serialization.h"

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
DogRepr::DogRepr(const model::Dog& dog)
    : id_(dog.GetId())
    , name_(dog.GetName())
    , pos_(dog.GetPos())
    , vel_(dog.GetVelocity())
    , dir_(dog.GetDir())
    , bag_capacity_(dog.GetBagCapacity())
    , bag_(dog.GetBag())
    , score_(dog.GetScore())
    , age_(dog.GetAge().count())
    , idle_for_(dog.GetIdleFor().count())
    , is_idle_(dog.IsIdle()) {
}

model::Dog DogRepr::Restore() const {
    model::Dog dog{name_, pos_, vel_, bag_capacity_, id_};
    dog.SetDir(dir_);
    dog.SetScore(score_);
    dog.SetAge(std::chrono::milliseconds{age_});
    dog.SetIdleFor(std::chrono::milliseconds{idle_for_});
    dog.SetIdle(is_idle_);
    for (const auto& item : bag_) {
        if (!dog.TryGrabItem(item.first, item.second)) {
            throw std::runtime_error("Failed to put bag content");
        }
    }
    return dog;
}

// LootGeneratorRepr
LootGeneratorRepr::LootGeneratorRepr(const loot_gen::LootGenerator& loot_gen)
    : time_without_loot_(loot_gen.GetTimeWithoutLoot().count()) {
}

void LootGeneratorRepr::Restore(model::GameSession& session) const {
    session.GetLootGenerator().SetTimeWithoutLoot(std::chrono::milliseconds{time_without_loot_});
}

// GameSessionRepr
GameSessionRepr::GameSessionRepr(const model::GameSession& game_session)
    : map_id_(*game_session.GetMap()->GetId())
    , loot_map_repr_(game_session.GetLoot())
    , loot_gen_repr_(game_session.GetLootGenerator())
    , loot_id_(game_session.GetNextLootId()) {
        for(const auto& [dog_id, dog] : game_session.GetDogs()) {
            dogs_repr_.emplace_back(*dog);
        }
}

model::GameSession GameSessionRepr::Restore(const app::Application& app) const {
    auto& game = app.GetGame();
    auto map = game.FindMap(model::Map::Id{map_id_});
    if (!map) {
        throw std::runtime_error("Map not found");
    }
    model::GameSession game_session(map, game.GetLootGenInterval(), game.GetLootGenProbability());
    for (const auto& dog_repr : dogs_repr_) {
        game_session.AddDog(std::make_shared<model::Dog>(dog_repr.Restore()));
    }
    for (const auto& [id, loot] : loot_map_repr_) {
        game_session.AddLoot(loot, id);
    }
    loot_gen_repr_.Restore(game_session);
    game_session.SetNextLootId(loot_id_);
    return game_session;
}

// GameRepr
GameRepr::GameRepr(const model::Game& game) {
    for (const auto& [map_id, session] : game.GetSessions()) {
        sessions_repr_.emplace_back(*session);
    }
}

void GameRepr::Restore(app::Application& app) const {
    auto& game = app.GetGame();
    for (const auto& session_repr : sessions_repr_) {
        game.AddSession(std::make_shared<model::GameSession>(session_repr.Restore(app)));
    }
}

// PlayerRepr
PlayerRepr::PlayerRepr(const model::Player& player)
    : session_id_(*player.GetSession()->GetMap()->GetId())
    , dog_id_(player.GetDog()->GetId()) {
}

model::Player PlayerRepr::Restore(app::Application& app) const {
    auto& game = app.GetGame();
    auto session_opt = game.FindSession(model::Map::Id{session_id_});
    if (!session_opt) {
        throw std::runtime_error("Session not found");
    }
    auto& session = session_opt.value().get();
    auto dog_opt = session->GetDogById(dog_id_);
    if (!dog_opt) {
        throw std::runtime_error("Dog not found");
    }
    auto& dog = dog_opt.value().get();
    return model::Player(session, dog);
}

// PlayersRepr
PlayersRepr::PlayersRepr(const app::Players& players) {
    for (const auto& [player_id, player] : players.GetPlayers()) {
        players_repr_.emplace_back(*player);
    }
}

void PlayersRepr::Restore(app::Application& app) const {
    for (const auto& player_repr : players_repr_) {
        auto player = player_repr.Restore(app);
        auto id = player.GetId();
        app.GetPlayers().GetPlayers().emplace(id, std::make_shared<model::Player>(std::move(player)));
    }
}

// PlayerTokensRepr
PlayerTokensRepr::PlayerTokensRepr(const app::PlayerTokens& player_tokens) {
    for (const auto& [token, player] : player_tokens.GetTokens()) {
        token_to_player_repr_.emplace_back(*token, player->GetId());
    }
}

void PlayerTokensRepr::Restore(app::Application& app) const {
    for (const auto& [token, player_id] : token_to_player_repr_) {
        auto player_opt = app.GetPlayers().FindPlayerById(player_id);
        if (player_opt) {
            app.GetTokens().AddTokenForPlayer(model::Token{token}, player_opt.value().get());
        } else {
            throw std::runtime_error("Player not found for token during deserialization");
        }
    }
}

// ApplicationRepr
ApplicationRepr::ApplicationRepr(const app::Application& app)
    : game_repr_(app.GetGame())
    , players_repr_(app.GetPlayers())
    , player_tokens_repr_(app.GetTokens()) {
}

void ApplicationRepr::Restore(app::Application& app) const {
    game_repr_.Restore(app);
    players_repr_.Restore(app);
    player_tokens_repr_.Restore(app);
}

}  // namespace serialization
