#pragma once

#include <boost/log/trivial.hpp>
#include <boost/json.hpp>

namespace logging
{

namespace boostlog = boost::log;
namespace json = boost::json;

void BootstrapLogging();
void LOG_INFO(json::value data, std::string message);

} // namespace log



