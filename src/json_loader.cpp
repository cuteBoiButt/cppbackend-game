#include "json_loader.h"
#include <boost/json.hpp>
#include <iterator>
#include <fstream>

namespace json_loader {

constexpr static boost::json::string_view ID  = "id";

constexpr static boost::json::string_view X  = "x";
constexpr static boost::json::string_view X1 = "x1";
constexpr static boost::json::string_view Y  = "y";

constexpr static boost::json::string_view W = "w";
constexpr static boost::json::string_view H = "h";

model::Office ParseOffice(boost::json::value& office_v) {
    auto& office_json = office_v.as_object();
    geom::Point pos{static_cast<geom::Coord>(office_json.at(X).as_int64()), static_cast<geom::Coord>(office_json.at(Y).as_int64())};
    geom::Offset off{static_cast<geom::Coord>(office_json.at("offsetX").as_int64()), static_cast<geom::Coord>(office_json.at("offsetY").as_int64())};
    return {model::Office::Id{std::string(office_json.at(ID).as_string())}, pos, off};
}

model::Building ParseBuilding(boost::json::value& building_v) {
    auto& building_json = building_v.as_object();
    geom::Point pos{static_cast<geom::Coord>(building_json.at(X).as_int64()), static_cast<geom::Coord>(building_json.at(Y).as_int64())};
    geom::Size size{static_cast<geom::Coord>(building_json.at(W).as_int64()), static_cast<geom::Coord>(building_json.at(H).as_int64())};
    return model::Building{{pos, size}};
}

std::unique_ptr<model::Road> ParseRoad(boost::json::value& road_v) {
    auto& road_json = road_v.as_object();
    geom::Point start{static_cast<geom::Coord>(road_json.at("x0").as_int64()), static_cast<geom::Coord>(road_json.at("y0").as_int64())};
    if(road_json.contains(X1)) {
        return std::make_unique<model::Road>(model::Road::HORIZONTAL, start, static_cast<geom::Coord>(road_json.at(X1).as_int64()));
    }
    return std::make_unique<model::Road>(model::Road::VERTICAL, start, static_cast<geom::Coord>(road_json.at("y1").as_int64()));
}

model::Map ParseMap(boost::json::value& map_v, double default_dog_speed, size_t default_bag_capacity) {
    auto& map_json = map_v.as_object();

    const auto dog_speed = map_json.contains("dogSpeed") ? map_json["dogSpeed"].as_double() : default_dog_speed;

    const auto bag_capacity = map_json.contains("bagCapacity") ? map_json["bagCapacity"].as_int64() : default_bag_capacity;

    const auto& loot_types = map_json.at("lootTypes").as_array();

    model::Map map{
        model::Map::Id{std::string(map_json.at(ID).as_string())}
      , std::string(map_json.at("name").as_string())
      , dog_speed
      , ExtraData{loot_types}
      , bag_capacity
      };

    for(auto& road_v : map_json.at("roads").as_array()) {
        map.AddRoad(ParseRoad(road_v));
    }

    for(auto& building_v : map_json.at("buildings").as_array()) {
        map.AddBuilding(ParseBuilding(building_v));
    }

    for(auto& office_v : map_json.at("offices").as_array()) {
        map.AddOffice(ParseOffice(office_v));
    }

    return map;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;

    std::ifstream ifs(json_path);

    if(!ifs.is_open()) {
        throw std::runtime_error(std::string("Could not open file ") + json_path.c_str());
    }

    std::string input(std::istreambuf_iterator<char>(ifs), {});

    auto root = boost::json::parse(input);

    const auto& gen_conf = root.as_object().at("lootGeneratorConfig").as_object();

    game.SetLootGenInterval(std::chrono::milliseconds(static_cast<int>(gen_conf.at("period").as_double())));
    game.SetLootGenProbability(gen_conf.at("probability").as_double());

    if(root.as_object().contains("defaultDogSpeed")) {
        game.SetDefaultDogSpeed(root.as_object()["defaultDogSpeed"].as_double());
    }

    if(root.as_object().contains("defaultBagCapacity")) {
        game.SetDefaultBagCapacity(root.as_object()["defaultBagCapacity"].as_int64());
    }

    if(root.as_object().contains("dogRetirementTime")) {
        game.SetMaxIdleTime(std::chrono::milliseconds{
                static_cast<size_t>(root.as_object()["dogRetirementTime"].as_double() * 1000.0)
        });
    }

    auto& maps = root.as_object().at("maps").as_array();

    for(auto& map_v : maps) {
        game.AddMap(ParseMap(map_v, game.GetDefaultDogSpeed(), game.GetDefaultBagCapacity()));
    }

    return game;
}

}  // namespace json_loader
