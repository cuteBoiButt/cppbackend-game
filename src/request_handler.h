#pragma once
#include "http_server.h"
#include "log.h"
#include "app.h"

#include <boost/json.hpp>

#include <filesystem>
#include <variant>
#include <chrono>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;
namespace fs = std::filesystem;


using StringRequest = http::request<http::string_body>;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

class httpException : public std::exception {
public:
    httpException(http::status status, std::string_view code, std::string_view message, std::vector<std::pair<std::string, std::string>> fields = {}) : 
    status_{status}, code_{code}, message_{message}, additional_fields_{std::move(fields)} {}

    const auto& GetStatus() const noexcept {
        return status_;
    }

    const auto& GetCode() const noexcept {
        return code_;
    }

    const auto& GetMessage() const noexcept {
        return message_;
    }

    const auto& GetAdditionalFields() const noexcept {
        return additional_fields_;
    }

    const char* what() const noexcept override  {
        return message_.c_str();
    }
private:
    http::status status_;
    std::string code_;
    std::string message_;

    std::vector<std::pair<std::string, std::string>> additional_fields_;
};

std::string UrlDecode(std::string_view str);
struct ParsedURL {
    std::string path;
    std::unordered_map<std::string, std::string> parameters;
};
ParsedURL ParseUrl(std::string_view url);

template<class SomeRequestHandler>
class LoggingRequestHandler {
    template<class Req, typename Endpoint>
    static void LogRequest(const Req& req, const Endpoint& ep) {
        using namespace std::literals;
        logging::LOG_INFO({{"ip", ep.address().to_string()}, {"URI", req.target()}, {"method", http::to_string(req.method())}}, "request received");
    }
    template<class Resp, typename Dur>
    static void LogResponse(const Resp& resp, Dur dur) {
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        json::object json_log{{"response_time", millis}, {"code", resp.result_int()}};
        if(resp.find(http::field::content_type) != resp.end()) {
            json_log["content_type"] = std::string(resp.at(http::field::content_type));
        } else {
            json_log["content_type"] = nullptr;
        }
        logging::LOG_INFO(std::move(json_log), "response sent");
    }
public:
    explicit LoggingRequestHandler(SomeRequestHandler&& handler) : decorated_{std::forward<SomeRequestHandler>(handler)} {}

    template <typename Request, typename Endpoint, typename Send>
    void operator()(Request&& req, const Endpoint& ep, Send&& send) {
        LogRequest(req, ep);
        const auto start_tp = std::chrono::system_clock::now(); 
        decorated_(std::forward<Request>(req), [start_tp, send = std::forward<Send>(send)](auto&& resp){
            LogResponse(resp, std::chrono::system_clock::now() - start_tp);
            send(std::forward<decltype(resp)>(resp));
        });
    }

private:
     SomeRequestHandler decorated_;
};

class ApiHandler {
public:
    explicit ApiHandler(app::Application& app, bool serve_tick_endpoint)
        : app_{app}, serve_tick_endpoint_{serve_tick_endpoint} {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    static bool isApiRequest(const StringRequest& req);
    StringResponse HandleApiRequest(const StringRequest& req, const ParsedURL& url);

private:
    model::Player* AuthPlayer(const StringRequest& req) const;

    StringResponse Join(const StringRequest& req);
    StringResponse GetPlayers(const StringRequest& req);
    StringResponse SetPlayerAction(const StringRequest& req);
    StringResponse GetGameState(const StringRequest& req);
    StringResponse GetMap(const StringRequest& req);
    StringResponse GetMaps(const StringRequest& req);
    StringResponse Tick(const StringRequest& req);
    StringResponse GetRecords(const StringRequest& req, const ParsedURL& url);

    app::Application& app_;
    bool serve_tick_endpoint_;
};

//что бы уж точно ничего лишнего не вылезло наружу, что мы случайно упустили
template<typename Func, typename ClassType, typename... Args>
auto RequestHandlerWrapper(ClassType* obj, Func func, Args&&... args) {
    using namespace std::literals;
    try {
        return std::invoke(func, obj, std::forward<Args>(args)...);
    } catch(const httpException&) {
        throw;
    } catch(const std::exception& e) {
        throw httpException(http::status::internal_server_error, "internalServerError"sv, e.what());
    }
}

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(app::Application& app, fs::path static_path, Strand api_strand, bool serve_tick_endpoint)
        : static_path_{static_path}, api_handler_{app, serve_tick_endpoint}, api_strand_{api_strand} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;

        auto version = req.version();
        auto keep_alive = req.keep_alive();

        try {
            auto parsed_url = ParseUrl(UrlDecode(req.target()));

            if(api_handler_.isApiRequest(req)) {
                auto handle = [self = shared_from_this(), send,
                               req = std::forward<decltype(req)>(req), version, keep_alive, parsed_url] {
                    try {
                        return send(RequestHandlerWrapper(&self->api_handler_, &ApiHandler::HandleApiRequest, req, parsed_url));
                    } catch (httpException&) {
                        send(self->ReportServerError(version, keep_alive));
                    }
                };
                return net::dispatch(api_strand_, handle);
            }
            // Возвращаем результат обработки запроса к файлу
            return std::visit(
                [&send](auto&& result) {
                    send(std::forward<decltype(result)>(result));
                },
                RequestHandlerWrapper(this, &RequestHandler::HandleFileRequest, req, parsed_url));
        } catch (httpException&) {
            send(ReportServerError(version, keep_alive));
        }
    }

private:
    using FileRequestResult = std::variant<StringResponse, FileResponse>;

    FileRequestResult HandleFileRequest(const StringRequest& req, const ParsedURL& url) const;
    StringResponse ReportServerError(unsigned version, bool keep_alive) const;

    fs::path static_path_;
    ApiHandler api_handler_;
    Strand api_strand_;
};

}  // namespace http_handler
