#pragma once

#include "app.h"
#include "log.h"

#include <iostream>

namespace retirement {

class RetirementListener : public app::ApplicationListener {
public:
    RetirementListener(app::Application& app)
        : app_(app) {}

    void OnTick([[maybe_unused]] std::chrono::milliseconds delta) override;

private:
    app::Application& app_;
};


}
