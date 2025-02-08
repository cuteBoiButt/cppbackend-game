#include "retirement.h"

namespace retirement {

void RetirementListener::OnTick([[maybe_unused]] std::chrono::milliseconds delta) {
    for(auto& [map_id, session] : app_.GetGame().GetSessions()) {
        for(auto dog_it = session->GetDogs().begin(); dog_it != session->GetDogs().end();) {
            auto& [dog_id, dog] = *dog_it;
            if(dog->GetIdleFor() >= app_.GetGame().GetMaxIdleTime()) {
                try {
                    //without valid transaction everything else is irrelevant
                    auto uow = app_.GetUoW();

                    model::RetiredDog retired_dog{model::RetiredDog::Id::New()
                        , std::string(dog->GetName())
                        , static_cast<int>(dog->GetScore())
                        , static_cast<int>(dog->GetAge().count())
                    };
                    uow->GetRetiredDogs().Save(retired_dog);

                    //commit first, before we mess up our game state
                    //faulty db connection should not result in data loss
                    uow->Commit();

                    //cleanup state
                    app_.GetTokens().RemoveToken(dog_id);
                    app_.GetPlayers().RemovePlayer(dog_id);
                    dog_it = session->GetDogs().erase(dog_it);
                } catch(const std::exception& e) {
                    logging::LOG_INFO({{"what", e.what()}}, "Error: Could not retire dog");
                    continue;
                }
            } else {
                ++dog_it;
            }
        }
    }
}

}
