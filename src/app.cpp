#include "app.h"
#include "json_loader.h"
#include "postgres.h"

namespace app {

using namespace std::literals;

const model::PlayerPtr& Players::Add(const model::DogPtr& dog, const model::GameSessionPtr& session) {
    return players_.emplace(dog->GetId(), std::make_shared<model::Player>(session, dog)).first->second;
}

std::optional<std::reference_wrapper<const model::PlayerPtr>> Players::FindPlayerById(size_t id) const {
    if (auto it = players_.find(id); it != players_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Players::RemovePlayer(size_t id) {
    return players_.erase(id) != 0;
}

std::optional<std::reference_wrapper<const model::PlayerPtr>> PlayerTokens::FindPlayerByToken(const model::Token& token) const noexcept {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return std::ref(it->second);
    }
    return std::nullopt;
}

model::Token PlayerTokens::AddPlayer(const model::PlayerPtr& player) {
    model::Token token{GenerateToken()};
    AddTokenForPlayer(token, player);
    return token;
}

void PlayerTokens::RemoveToken(size_t player_id) {
    const auto& token = player_id_to_token_.at(player_id);
    token_to_player_.erase(token);
    player_id_to_token_.erase(player_id);
}

void PlayerTokens::AddTokenForPlayer(const model::Token& token, const model::PlayerPtr& player) {
    token_to_player_[token] = player;
    player_id_to_token_.emplace(player->GetId(), token);
}

std::string PlayerTokens::GenerateToken() {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(16) << std::hex << generator1_();
    stream << std::setfill('0') << std::setw(16) << std::hex << generator2_();
    return stream.str();
}

Application::Application(const std::filesystem::path& json_path, bool randomize_spawns, std::unique_ptr<db::Database, void(*)(db::Database*)> db)
                         : game_{json_loader::LoadGame(json_path)}
                         , random_spawns_{randomize_spawns}
                         , database_{std::move(db)} {}

const model::Game::Maps& Application::ListMaps() const noexcept {
    return game_.GetMaps();
}

const model::Map* Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
}

std::pair<const model::PlayerPtr&, model::Token> Application::JoinGame(const model::Map::Id& map_id, std::string_view user_name) {
    auto session_opt = game_.GetSession(map_id);
    if(!session_opt) throw std::runtime_error("null session");
    auto& session = session_opt.value().get();

    auto map = session->GetMap();

    geom::Point2D pos;
    if(random_spawns_) { //select randowm starting location
        std::uniform_int_distribution<int> uni(0,map->GetRoads().size()-1);
        const auto& road = map->GetRoads()[uni(rand_)];
        if(road->IsHorizontal()) {
            std::uniform_real_distribution<double> uni2(road->GetStart().x, road->GetEnd().x);
            pos.x = uni2(rand_);
            pos.y = road->GetStart().y;
        } else {
            std::uniform_real_distribution<double> uni2(road->GetStart().y, road->GetEnd().y);
            pos.y = uni2(rand_);
            pos.x = road->GetStart().x;
        }
    } else {
        auto start = map->GetRoads().front()->GetStart();
        pos = {static_cast<double>(start.x), static_cast<double>(start.y)};
    }

    auto& dog = session->CreateDog(user_name, pos);
    auto& player = players_.Add(dog, session);
    auto token = tokens_.AddPlayer(player);
    return {player, std::move(token)};
}

std::optional<std::reference_wrapper<const model::PlayerPtr>> Application::FindPlayerByToken(const model::Token& token) {
    return tokens_.FindPlayerByToken(token);
}

void Application::SetPlayerAction(model::Player* player, std::string_view action) {
    auto dog = player->GetDog();
    auto dog_speed = player->GetSession()->GetMap()->GetDogSpeed();

    if(action == "L"sv) {
        dog->SetIdle(false);
        dog->SetDir(model::Direction::WEST);
        dog->SetVelocity({-dog_speed, 0.0});
    } else if(action == "R"sv) {
        dog->SetIdle(false);
        dog->SetDir(model::Direction::EAST);
        dog->SetVelocity({dog_speed, 0.0});
    } else if(action == "U"sv) {
        dog->SetIdle(false);
        dog->SetDir(model::Direction::NORTH);
        dog->SetVelocity({0.0, -dog_speed});
    } else if(action == "D"sv) {
        dog->SetIdle(false);
        dog->SetDir(model::Direction::SOUTH);
        dog->SetVelocity({0.0, dog_speed});
    } else {
        dog->SetIdle(true);
        dog->SetVelocity({0.0, 0.0});
    }
}

geom::Point2D Application::GetRandomPointOnMap(const model::Map* map) {
    std::uniform_int_distribution<> road_dist(0, map->GetRoads().size() - 1);
    const auto& road = map->GetRoads()[road_dist(rand_)];
    std::uniform_real_distribution<> x_dist(std::min(road->GetStart().x, road->GetEnd().x), std::max(road->GetStart().x, road->GetEnd().x));
    std::uniform_real_distribution<> y_dist(std::min(road->GetStart().y, road->GetEnd().y), std::max(road->GetStart().y, road->GetEnd().y));
    return {x_dist(rand_), y_dist(rand_)};
}

class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    VectorItemGathererProvider(const std::unordered_map<size_t, std::pair<size_t, geom::Point2D>>& loot,
                               const std::vector<collision_detector::Item>& bases,
                               const std::vector<std::pair<model::Dog*, collision_detector::Gatherer>>& gatherers) : gatherers_(gatherers) {
        loot_data_.reserve(loot.size());
        items_.reserve(loot.size() + bases.size());
        for(const auto& [id, item] : loot) {
            loot_data_.emplace_back(id, item.first);
            items_.emplace_back(item.second, 0.0);
        }
        bases_offset_ = items_.size();
        items_.insert(items_.end(), bases.begin(), bases.end());
    }

    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx).second;
    }

    bool IsItemIdx(size_t idx) const {
        return bases_offset_ != 0 && idx < bases_offset_;
    }
    auto& GetLootData(size_t idx) const {
        return loot_data_.at(idx);
    }

    model::Dog* GetDog(size_t idx) const {
        return gatherers_.at(idx).first;
    }

private:
    std::vector<collision_detector::Item> items_;
    const std::vector<std::pair<model::Dog*, collision_detector::Gatherer>>& gatherers_;
    size_t bases_offset_{0};
    std::vector<std::pair<size_t,size_t>> loot_data_;
};

std::unique_ptr<db::UnitOfWork, void(*)(db::UnitOfWork*)> Application::GetUoW() {
    return database_->GetUoW();
}

void Application::AddListener(const std::shared_ptr<ApplicationListener>& listener) {
    listeners_.emplace_back(listener);
}

void Application::Tick(std::chrono::milliseconds dt) {
    for(const auto& p : game_.GetSessions()) {
        const auto& session = p.second;
        auto map = session->GetMap();
        
        //Generate new loot
        {
            auto n_new_loot = session->GenerateLoot(dt);

            std::uniform_int_distribution<> loot_obj_distrib(0, map->GetExtraData().GetLootTypes().size() - 1);

            for(size_t i = 0; i < n_new_loot; ++i) {
                session->AddLoot({loot_obj_distrib(rand_), GetRandomPointOnMap(map)});
            }
        }

        std::vector<std::pair<model::Dog*, collision_detector::Gatherer>> gatherers;

        //Move and tick dogs
        {
            for(auto& [dog_id, dog] : session->GetDogs()) {
                if(dog->GetIdleFor() >= game_.GetMaxIdleTime()) {
                    //этого быть не должно, но если случилось - то значит что-то с бд
                    //эти собаки уже "протухли", поэтому мы их не тикаем
                    //чтобы сохранить их состояние в целости до момента как сможем сохранить их результат в бд
                    continue;
                }

                //increase age and idle time
                dog->SetAge(dog->GetAge() + dt);
                if(dog->IsIdle()) {
                    dog->SetIdleFor(dog->GetIdleFor() + dt);
                } else {
                    dog->SetIdleFor(std::chrono::milliseconds{0});
                }

                //move
                auto old_pos = dog->GetPos();
                map->MoveDog(dog.get(), dt);
                gatherers.emplace_back(dog.get(), collision_detector::Gatherer{old_pos, dog->GetPos(), 0.6});
            }
        }

        //Do item gathering
        {
            VectorItemGathererProvider provider(session->GetLoot(), map->GetExtraData().GetBases(), gatherers);
            auto collision_events = collision_detector::FindGatherEvents(provider);
            
            for(const auto& e : collision_events) {
                auto dog = provider.GetDog(e.gatherer_id);

                if(provider.IsItemIdx(e.item_id)) {
                    auto [loot_id, loot_type] = provider.GetLootData(e.item_id);
                    if(dog->TryGrabItem(loot_id, loot_type)) {
                        session->RemoveLoot(loot_id);
                    }
                } else {
                    for(const auto& item : dog->GetBag()) {
                        auto type = item.second;
                        auto value = map->GetExtraData().GetLootTypes().at(type).as_object().at("value").as_int64();
                        dog->SetScore(dog->GetScore() + value);
                    }
                    dog->ClearBag();
                }
            }
        }

        //Notify
        {
            for (auto it = listeners_.begin(); it != listeners_.end(); ) {
                if(auto ptr = it->lock()) {
                    ptr->OnTick(dt);
                    ++it;
                } else {
                    it = listeners_.erase(it);
                }
            }
        }
    }
}

}
