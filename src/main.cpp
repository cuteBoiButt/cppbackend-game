#include "sdk.h"
#include "log.h"
#include "app.h"
#include "request_handler.h"
#include "serialization.h"
#include "retirement.h"
#include "postgres.h"
#include "ticker.h"

#include <boost/program_options.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/optional/optional_io.hpp>

#include <iostream>
#include <thread>
#include <optional>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace beast = boost::beast;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

struct Args {
    boost::optional<int> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points;
    boost::optional<std::string> state_file;
    boost::optional<int> save_state_period;
}; 

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"s)->default_value(boost::none, ""), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points)->default_value(false, ""), "spawn dogs at random positions")
        ("state-file", po::value(&args.state_file)->value_name("file"s), "set state file path")
        ("save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"s)->default_value(boost::none, ""), "set save state period");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file have not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files dir have not been specified"s);
    }

    if(args.tick_period && *args.tick_period <= 0) {
        throw std::runtime_error("tick-period must be > 0"s);
    }

    if(args.save_state_period && *args.save_state_period <= 0) {
        throw std::runtime_error("save-state-period must be > 0"s);
    }

    return args;
}

int main(int argc, const char* argv[]) {
    try {
        logging::BootstrapLogging();

        auto args = ParseCommandLine(argc, argv);
        if(!args) {
            return EXIT_SUCCESS;
        }
    
        std::string db_url;

        if (const auto* url = std::getenv(DB_URL_ENV_NAME)) {
            db_url = url;
        } else {
            throw std::runtime_error(DB_URL_ENV_NAME + " environment variable not found"s);
        }

        // 1. Загружаем карту из файла и построить модель игры
        app::Application application{args->config_file
                , args->randomize_spawn_points
                , postgres::CreateDatabaseImpl(1, [db_url] { return std::make_shared<pqxx::connection>(db_url); })
        };

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        // strand для выполнения запросов к API
        auto api_strand = net::make_strand(ioc);

        if(args->tick_period) {
            auto ticker = std::make_shared<app::Ticker>(api_strand, std::chrono::milliseconds(*args->tick_period),
                [&application](std::chrono::milliseconds delta) { application.Tick(delta); }
            );
            ticker->Start();
        }

        auto handler = std::make_shared<http_handler::RequestHandler>(application, args->www_root, api_strand, !args->tick_period.has_value());
        http_handler::LoggingRequestHandler logging_handler{
            [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        }};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& endpoint, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(endpoint)>(endpoint), std::forward<decltype(send)>(send));
        });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        logging::LOG_INFO({{"port", port}, {"address", address.to_string()}}, "server started");

        //Пытаемся загрузить состояние, если оно есть на диске
        serialization::LoadState(application, args->state_file);

        //Создаём и вешаем сериализующий Listener
        std::shared_ptr<serialization::SerializingListener> save_listener;

        if(args->state_file) {
            save_listener = std::make_shared<serialization::SerializingListener>(application, *args->state_file, args->save_state_period.get_value_or(-1));
            if(args->save_state_period) {
                application.AddListener(save_listener);
            }
        }
        auto retire_listener = std::make_shared<retirement::RetirementListener>(application);
        application.AddListener(retire_listener);

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        //В этой точке все асинхронные операции уже выполнены, можно спокойно сохранять
        if(save_listener) {
            save_listener->SaveState();
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
