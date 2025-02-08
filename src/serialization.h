#pragma once

#include "app.h"
#include "model_serialization.h"

#include <boost/optional.hpp>

#include <fstream>

namespace serialization {

class SerializingListener : public app::ApplicationListener {
public:
    SerializingListener(const app::Application& app, const std::filesystem::path& save_path, int save_interval)
        : app_(app), save_path_(save_path), save_interval_(save_interval) {}

    void OnTick(std::chrono::milliseconds delta) override;
    void SaveState();

private:
    const app::Application& app_;
    std::filesystem::path save_path_;
    std::chrono::milliseconds time_since_last_save_{0};
    const int save_interval_;
};

void LoadState(app::Application& app, const boost::optional<std::string>& path);

}  // namespace serialization
