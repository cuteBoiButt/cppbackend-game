#include "log.h"

#include <boost/log/expressions/keyword.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace logging
{

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(custom_message, "CustomMessage", std::string)

void LogFormatter(boostlog::record_view const& rec, boostlog::formatting_ostream& strm) {
    auto ts = *rec[timestamp];
    json::value data{
        {"timestamp", to_iso_extended_string(ts)},
        {"data",  *rec[additional_data]},
        {"message", *rec[custom_message]}
    };

    strm << json::serialize(data);
}

void LOG_INFO(json::value data, std::string message) {
    BOOST_LOG_TRIVIAL(info) << boostlog::add_value(timestamp, boost::posix_time::microsec_clock::local_time())
                            << boostlog::add_value(additional_data, data)
                            << boostlog::add_value(custom_message, message);
}

void BootstrapLogging() {
    auto sink = boostlog::add_console_log();
    sink->locked_backend()->auto_flush(true);
    sink->set_formatter(&LogFormatter);
}

} // namespace log
