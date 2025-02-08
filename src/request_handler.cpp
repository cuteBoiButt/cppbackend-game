#include "request_handler.h"
#include <boost/algorithm/string.hpp>

#include <exception>
#include <unordered_map>
#include <optional>

namespace http_handler {

namespace sys = boost::system;
using namespace std::literals;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
};

std::string_view ContentTypeFromExtension(fs::path path) {
    auto extension = path.extension().string();
    boost::algorithm::to_lower(extension);

    if(extension == ".htm"sv || extension == ".html"sv) return "text/html"sv;
    else if(extension == ".css"sv) return "text/css"sv;
    else if(extension == ".txt"sv) return "text/plain"sv;
    else if(extension == ".js"sv) return "text/javascript"sv;
    else if(extension == ".json"sv) return "application/json"sv;
    else if(extension == ".xml"sv) return "application/xml"sv;
    else if(extension == ".png"sv) return "image/png"sv;
    else if(extension == ".jpg" || extension == ".jpe"sv || extension == ".jpeg"sv) return "image/jpeg"sv;
    else if(extension == ".gif"sv) return "image/gif"sv;
    else if(extension == ".bmp"sv) return "image/bmp"sv;
    else if(extension == ".ico"sv) return "image/vnd.microsoft.icon"sv;
    else if(extension == ".tiff"sv || extension == ".tif"sv) return "image/tiff"sv;
    else if(extension == ".svg"sv || extension == ".svgz"sv) return "image/svg+xml"sv;
    else if(extension == ".mp3"sv) return "audio/mpeg"sv;
    else return "application/octet-stream"sv;
}

bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::string UrlDecode(std::string_view str) {
    std::string decoded;
    decoded.reserve(str.size());

    for (auto i = str.begin(), nd = str.end(); i != nd; ++i) {
        char c = *i;

        if (c == '%') {
            if (i + 2 >= nd) {
                throw std::invalid_argument("Invalid escape sequence: not enough characters after '%'");
            }

            //*i - '%', *(i + 1) - первый байт экодированной последовательности, *(i + 2) - второй
            //NULL в конце, ибо std::strtol ожидает на входе "null-terminated byte string"
            char hex[3] = { *(i + 1), *(i + 2), '\0' };
            char* end; //нужно чтобы удостовериться что мы "съели" оба байта
            auto value = std::strtol(hex, &end, 16);

            if (end != &hex[2] || value > 255 || value < 0) {
                throw std::invalid_argument("Invalid escape sequence: non-hexadecimal characters after '%'");
            }

            decoded += static_cast<char>(value);
            i += 2;
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }

    return decoded;
}

ParsedURL ParseUrl(std::string_view url) {
    ParsedURL result{};
    auto query_pos = url.find('?');
    if (url.find('?', query_pos + 1) != std::string_view::npos) {
        throw httpException(http::status::bad_request, "badRequest"sv, "Multiple '?' in path"sv, {{"Cache-Control"s, "no-cache"s}});
    }
    if (query_pos != std::string_view::npos) {
        auto query = url.substr(query_pos + 1);
        std::vector<std::string_view> pairs;
        boost::algorithm::split(pairs, query, boost::is_any_of("&"));
        for (const auto& pair : pairs) {
            std::vector<std::string_view> kv;
            boost::algorithm::split(kv, pair, boost::is_any_of("="));
            if (kv.empty()) continue;
            auto key = kv[0];
            if (key.empty()) {
                throw httpException(http::status::bad_request, "badRequest"sv, "Empty key in parameters"sv, {{"Cache-Control"s, "no-cache"s}});
            }
            auto value = kv.size() > 1 ? kv[1] : ""sv;
            result.parameters.emplace(key, value);
        }
    }
    result.path = url.substr(0, query_pos);
    if (!result.path.empty() && result.path.front() == '/') {
        result.path.erase(0, 1);
    }
    return result;
}

StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML,
                                  bool is_head = false,
                                  std::vector<std::pair<std::string, std::string>> fields = {}) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.keep_alive(keep_alive);

    if(!is_head) {
        response.body() = body;
    }
    response.content_length(body.size());

    for(const auto& p : fields) {
        response.set(p.first, p.second);
    }

    return response;
}

StringResponse MakeJsonResponse(const StringRequest& req, http::status status, std::string_view json, std::vector<std::pair<std::string, std::string>> fields = {}) {
    return MakeStringResponse(status, json, req.version(), req.keep_alive(), ContentType::APPLICATION_JSON, req.method() == http::verb::head, std::move(fields));
}

FileResponse MakeFileResponse(http::status status, fs::path path, unsigned http_version,
                                  bool keep_alive,
                                  bool is_head = false,
                                  std::vector<std::pair<std::string, std::string>> fields = {}) {
    FileResponse response(status, http_version);
    response.set(http::field::content_type, ContentTypeFromExtension(path));
    response.keep_alive(keep_alive);

    if(!is_head) {
        http::file_body::value_type file;
        if (sys::error_code ec; file.open(path.c_str(), beast::file_mode::read, ec), ec) {
            throw httpException(http::status::internal_server_error, "internalServerError", ec.what());
        }
        response.body() = std::move(file);
        response.prepare_payload();
    } else {
        response.content_length(fs::file_size(path));
    }

    for(const auto& p : fields) {
        response.set(p.first, p.second);
    }

    return response;
}

StringResponse RequestHandler::ReportServerError(unsigned version, bool keep_alive) const {
    const auto json_response = [version, keep_alive](http::status status, std::string_view json, std::vector<std::pair<std::string, std::string>> fields) {
        return MakeStringResponse(status, json, version, keep_alive, ContentType::APPLICATION_JSON, false, std::move(fields));
    };

    try {
        std::rethrow_exception(std::current_exception());
    } catch(const httpException& e) {
        boost::json::object resp_js{};
        resp_js["code"] = e.GetCode();
        resp_js["message"] = e.GetMessage();
        return json_response(e.GetStatus(), boost::json::serialize(resp_js), e.GetAdditionalFields());
    }
}

RequestHandler::FileRequestResult RequestHandler::HandleFileRequest(const StringRequest& req, const ParsedURL& url) const {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "methodNotAllowed"sv, "Method not allowed"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }

    const auto file_response = [&req, is_head](http::status status, fs::path path) {
        return MakeFileResponse(status, path, req.version(), req.keep_alive(), is_head);
    };
    const auto text_response = [&req, is_head](http::status status, std::string_view body, std::vector<std::pair<std::string, std::string>> fields) {
        return MakeStringResponse(status, body, req.version(), req.keep_alive(), ContentType::TEXT_PLAIN, false, std::move(fields));
    };

    auto req_path = url.path;
    if(!req_path.size() || req_path.back() == '/') {
        req_path += "index.html"sv;
    }
    const auto path = static_path_/req_path;
    if(IsSubPath(path, static_path_) && fs::exists(path)) {
        return file_response(http::status::ok, path);
    }
    return text_response(http::status::not_found, "File not found", {{"Cache-Control"s, "no-cache"s}});
}

std::optional<std::string_view> GetField(const StringRequest& req, std::string_view field) {
    try {
        return req.at(field);
    } catch(std::exception&) {
        return std::nullopt;
    }
}

void EnsureCorrectCT(const StringRequest& req, std::string_view ct_sv) {
    auto ct = GetField(req, "Content-Type"sv);
    if(!ct || *ct != ct_sv) {
        throw httpException(http::status::bad_request, "invalidArgument"sv, "Invalid content type"sv, {{"Cache-Control"s, "no-cache"s}});
    }
}

auto ParseJsonBody(const StringRequest& req) {
    return json::parse(req.body());
}

auto ParseAuthToken(const StringRequest& req) {
    auto auth = GetField(req, "Authorization"sv);
    if(!auth || !auth->starts_with("Bearer "sv) || auth->size() != 39) {
        throw httpException(http::status::unauthorized, "invalidToken"sv, "Authorization header is missing"sv, {{"Cache-Control"s, "no-cache"s}});
    }
    return model::Token{std::string(auth->substr(7))};
}

model::Player* ApiHandler::AuthPlayer(const StringRequest& req) const {
    auto token = ParseAuthToken(req);
    auto player = app_.FindPlayerByToken(token);
    if(!player) {
        throw httpException(http::status::unauthorized, "unknownToken"sv, "Player token has not been found"sv, {{"Cache-Control"s, "no-cache"s}});
    }
    return player.value().get().get();
}

bool ApiHandler::isApiRequest(const StringRequest& req) {
    return req.target().starts_with("/api/"sv);
}

StringResponse ApiHandler::Join(const StringRequest& req) {
    const bool is_post = req.method() == http::verb::post;

    if(!is_post) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Only POST method is expected"sv, {{"Allow"s, "POST"s}, {"Cache-Control"s, "no-cache"s}});
    }

    EnsureCorrectCT(req, "application/json"sv);

    json::value post_json;
    std::string_view user_name;
    std::string_view map_id_sv;

    try {
        post_json = ParseJsonBody(req);
        auto& post_json_obj = post_json.as_object();
        user_name = post_json_obj.at("userName").as_string();
        map_id_sv = post_json_obj.at("mapId").as_string();
    } catch(std::exception&) {
        throw httpException(http::status::bad_request, "invalidArgument"sv, "Join game request parse error"sv, {{"Cache-Control"s, "no-cache"s}});
    }

    if(user_name.size() == 0) {
        throw httpException(http::status::bad_request, "invalidArgument"sv, "Invalid name"sv, {{"Cache-Control"s, "no-cache"s}});
    }
    
    model::Map::Id map_id{std::string(map_id_sv)};

    if(app_.FindMap(map_id) == nullptr) {
        throw httpException(http::status::not_found, "mapNotFound"sv, "Map not found"sv, {{"Cache-Control"s, "no-cache"s}});
    }

    auto join_res = app_.JoinGame(map_id, user_name);

    json::object resp_js;
    resp_js["authToken"] = *join_res.second;
    resp_js["playerId"] = join_res.first->GetId();

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(resp_js), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::GetPlayers(const StringRequest& req) {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Invalid method"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }

    AuthPlayer(req);

    json::object resp_js{};

    for(const auto& [p_id, p] : app_.GetPlayers().GetPlayers()) {
        resp_js[std::to_string(p_id)] = json::object{{"name"s, p->GetName()}};
    }

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(resp_js), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::GetGameState(const StringRequest& req) {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Invalid method"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }

    auto player = AuthPlayer(req);

    json::object resp_js{};
    
    json::object players_js{};
    for(const auto& [dog_id, dog] : player->GetSession()->GetDogs()) {
        json::object dog_js{};
        dog_js["pos"] = json::array{dog->GetPos().x, dog->GetPos().y};
        dog_js["speed"] = json::array{dog->GetVelocity().x, dog->GetVelocity().y};
        dog_js["dir"] = std::string(model::DIR_TO_STRING[static_cast<size_t>(dog->GetDir())]);

        json::array bag_js{};
        for(const auto& [id, type] : dog->GetBag()) {
            json::object item_js{};
            item_js["id"] = id;
            item_js["type"] = type;
            bag_js.emplace_back(std::move(item_js));
        }
        dog_js["bag"] = std::move(bag_js);

        dog_js["score"] = dog->GetScore();

        players_js[std::to_string(dog->GetId())] = std::move(dog_js);
    }
    resp_js["players"] = std::move(players_js);

    json::object lost_obj_js{};
    for(const auto& [id, type_pos] : player->GetSession()->GetLoot()) {
        json::object obj_js{};
        obj_js["type"] = type_pos.first;
        obj_js["pos"] = json::array{type_pos.second.x, type_pos.second.y};
        lost_obj_js[std::to_string(id)] = std::move(obj_js);
    }
    resp_js["lostObjects"] = std::move(lost_obj_js);

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(resp_js), {{"Cache-Control"s, "no-cache"s}});
}

boost::json::array SerializeRoads(const std::vector<std::unique_ptr<model::Road>>& roads) {
    boost::json::array roads_array{};
    for(const auto& road : roads) {
        boost::json::object road_obj{};
        road_obj["x0"] = road->GetStart().x;
        road_obj["y0"] = road->GetStart().y;
        if(road->IsHorizontal()) {
            road_obj["x1"] = road->GetEnd().x;
        } else {
            road_obj["y1"] = road->GetEnd().y;
        }
        roads_array.emplace_back(std::move(road_obj));
    }
    return roads_array;
}

boost::json::array SerializeBuildings(const std::vector<model::Building>& buildings) {
    boost::json::array buildings_array{};
    for(const auto& building : buildings) {
        boost::json::object building_obj{};
        building_obj["x"] = building.GetBounds().position.x;
        building_obj["y"] = building.GetBounds().position.y;
        building_obj["w"] = building.GetBounds().size.width;
        building_obj["h"] = building.GetBounds().size.height;
        buildings_array.emplace_back(std::move(building_obj));
    }
    return buildings_array;
}

boost::json::array SerializeOffices(const std::vector<model::Office>& offices) {
    boost::json::array offices_array{};
    for(const auto& office : offices) {
        boost::json::object office_obj{};
        office_obj["id"] = *office.GetId();
        office_obj["x"] = office.GetPosition().x;
        office_obj["y"] = office.GetPosition().y;
        office_obj["offsetX"] = office.GetOffset().dx;
        office_obj["offsetY"] = office.GetOffset().dy;
        offices_array.emplace_back(std::move(office_obj));
    }
    return offices_array;
}

boost::json::object SerializeMap(const model::Map& map) {
    boost::json::object out_obj{};
    out_obj["id"] = *map.GetId();
    out_obj["name"] = map.GetName();
    out_obj["roads"] = SerializeRoads(map.GetRoads());
    out_obj["buildings"] = SerializeBuildings(map.GetBuildings());
    out_obj["offices"] = SerializeOffices(map.GetOffices());
    out_obj["lootTypes"] = map.GetExtraData().GetLootTypes();
    return out_obj;
}

StringResponse ApiHandler::GetMap(const StringRequest& req) {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Invalid method"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }

    auto target_map = req.target().substr(13);
    const auto map = app_.FindMap(model::Map::Id{std::string(target_map)});

    if(map == nullptr) {
        throw httpException(http::status::not_found, "mapNotFound"sv, "Map not found"sv, {{"Cache-Control"s, "no-cache"s}});
    }

    auto serialized_map = SerializeMap(*map);
    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(serialized_map), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::GetMaps(const StringRequest& req) {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "methodNotAllowed"sv, "Method not allowed"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }

    boost::json::array out_arr{};
    for(const auto& map : app_.ListMaps()) {
        boost::json::object obj{};
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        out_arr.emplace_back(std::move(obj));
    }

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(out_arr), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::SetPlayerAction(const StringRequest& req) {
    const bool is_post = req.method() == http::verb::post;

    if(!is_post) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Only POST method is expected"sv, {{"Allow"s, "POST"s}, {"Cache-Control"s, "no-cache"s}});
    }

    auto player = AuthPlayer(req);

    EnsureCorrectCT(req, "application/json"sv);

    json::value post_json;
    std::string_view move;

    try {
        post_json = ParseJsonBody(req);
        auto& post_json_obj = post_json.as_object();
        move = post_json_obj.at("move").as_string();
        if(move != "L"sv && move != "R"sv && move != "U"sv && move != "D"sv && move != ""sv) {
            throw std::exception();
        }
    } catch(std::exception&) {
        throw httpException(http::status::bad_request, "invalidArgument"sv, "Failed to parse action"sv, {{"Cache-Control"s, "no-cache"s}});
    }

    app_.SetPlayerAction(player, move);

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(json::object{}), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::Tick(const StringRequest& req) {
    const bool is_post = req.method() == http::verb::post;

    if(!is_post) {
        throw httpException(http::status::method_not_allowed, "invalidMethod"sv, "Only POST method is expected"sv, {{"Allow"s, "POST"s}, {"Cache-Control"s, "no-cache"s}});
    }

    EnsureCorrectCT(req, "application/json"sv);

    json::value post_json;
    int dt;

    try {
        post_json = ParseJsonBody(req);
        auto& post_json_obj = post_json.as_object();
        dt = post_json_obj.at("timeDelta").as_int64();
    } catch(std::exception&) {
        throw httpException(http::status::bad_request, "invalidArgument"sv, "Failed to parse tick request JSON"sv, {{"Cache-Control"s, "no-cache"s}});
    }

    app_.Tick(std::chrono::milliseconds(dt));

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(json::object{}), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::GetRecords(const StringRequest& req, const ParsedURL& url) {
    const bool is_get = req.method() == http::verb::get;
    const bool is_head = req.method() == http::verb::head;

    if(!is_get && !is_head) {
        throw httpException(http::status::method_not_allowed, "methodNotAllowed"sv, "Method not allowed"sv, {{"Allow"s, "GET, HEAD"s}, {"Cache-Control"s, "no-cache"s}});
    }
    //defaults
    int offset = 0;
    int limit = 100;

    if(url.parameters.contains("start")) {
        offset = std::stoi(url.parameters.at("start"));
        if(offset < 0) {
            throw httpException(http::status::bad_request, "badRequest"sv, "\"start\" out of range"sv, {{"Cache-Control"s, "no-cache"s}});
        }
    }

    if(url.parameters.contains("maxItems")) {
        limit = std::stoi(url.parameters.at("maxItems"));
        if(limit <= 0 || limit > 100) {
            throw httpException(http::status::bad_request, "badRequest"sv, "\"maxItems\" out of range"sv, {{"Cache-Control"s, "no-cache"s}});
        }
    }

    auto uow = app_.GetUoW();
    auto retired_dogs = uow->GetRetiredDogs().FetchRange(offset, limit);
    uow->Commit();

    boost::json::array out_arr{};
    for(const auto& retired_dog : retired_dogs) {
        boost::json::object obj{};
        obj["name"] = retired_dog.GetName();
        obj["score"] = retired_dog.GetScore();
        obj["playTime"] = ((double)retired_dog.GetPlayTime())/1000.0;
        out_arr.emplace_back(std::move(obj));
    }

    return MakeJsonResponse(req, http::status::ok, boost::json::serialize(out_arr), {{"Cache-Control"s, "no-cache"s}});
}

StringResponse ApiHandler::HandleApiRequest(const StringRequest& req, const ParsedURL& url) {
    if(url.path == "api/v1/maps"sv) {
        return GetMaps(req);
    } else if(url.path.starts_with("api/v1/maps/"sv)) {
        return GetMap(req);
    } else if(url.path == "api/v1/game/join"sv) {
        return Join(req);
    } else if(url.path == "api/v1/game/records"sv) {
        return GetRecords(req, url);
    } else if(url.path == "api/v1/game/players"sv) {
        return GetPlayers(req);
    } else if(url.path == "api/v1/game/state"sv) {
        return GetGameState(req);
    } else if(url.path == "api/v1/game/player/action"sv) {
        return SetPlayerAction(req);
    } else if(url.path == "api/v1/game/tick"sv && serve_tick_endpoint_) {
        return Tick(req);
    }
    throw httpException(http::status::bad_request, "badRequest"sv, "Invalid endpoint"sv);
}

}  // namespace http_handler
