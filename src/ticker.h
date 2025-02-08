#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <memory>

namespace app {

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler);

    void Start();

private:
    void ScheduleTick();
    void OnTick(boost::system::error_code ec);

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    boost::asio::steady_timer timer_;
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace app
