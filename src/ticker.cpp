#include "ticker.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <cassert>

namespace app {

Ticker::Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
    : strand_{strand}
    , period_{period}
    , timer_{strand_}
    , handler_{std::move(handler)} {
}

void Ticker::Start() {
    boost::asio::dispatch(strand_, [self = shared_from_this()] {
        self->last_tick_ = Clock::now();
        self->ScheduleTick();
    });
}

void Ticker::ScheduleTick() {
    assert(strand_.running_in_this_thread());
    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](boost::system::error_code ec) {
        self->OnTick(ec);
    });
}

void Ticker::OnTick(boost::system::error_code ec) {
    using namespace std::chrono;
    assert(strand_.running_in_this_thread());

    if (!ec) {
        auto this_tick = Clock::now();
        auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
        last_tick_ = this_tick;
        try {
            handler_(delta);
        } catch (std::exception&) {
            // Теоретически этого не должно произойти
            // Пользователь класса Ticker должен предоставить handler, который корректно обработает все exception'ы
            // Но если мы всё же тут оказались, то проигнорируем это исключение
        }
        ScheduleTick();
    }
}

} // namespace app
