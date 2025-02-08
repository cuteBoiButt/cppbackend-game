#include "serialization.h"
#include "log.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace serialization {

void SerializingListener::OnTick(std::chrono::milliseconds delta) {
    if(save_interval_ >= 0) {
        time_since_last_save_ += delta;
        if (time_since_last_save_ >= std::chrono::milliseconds{save_interval_}) {
            SaveState();
            time_since_last_save_ = std::chrono::milliseconds(0);
        }
    }
}

void SerializingListener::SaveState() {
    try {
        // Создаем временный файл с префиксом "temp_"
        std::filesystem::path temp_path = save_path_;
        temp_path = temp_path.parent_path() / ("temp_" + temp_path.filename().string());

        std::ofstream ofs(temp_path, std::ios::trunc);
        if (!ofs) {
            throw std::runtime_error("Failed to open temporary save file");
        }

        ApplicationRepr app_repr(app_);
        try {
            boost::archive::text_oarchive oa{ofs};
            oa << app_repr;
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to serialize application state: " + std::string(e.what()));
        }

        // Закрываем файл перед переименованием
        ofs.close();

        // Переименовываем временный файл в итоговый
        try {
            std::filesystem::rename(temp_path, save_path_);
        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error("Failed to rename temporary save file: " + std::string(e.what()));
        }
    } catch(const std::exception& e) {
        logging::LOG_INFO({{"what", e.what()}}, "Exception during serialization");
    }
}

void LoadState(app::Application& app, const boost::optional<std::string>& path) {
    if(path && std::filesystem::exists(*path)) {
        try {
            std::ifstream ifs(*path);
            if (!ifs) {
                throw std::runtime_error("Failed to open save file");
            }
            boost::archive::text_iarchive ia{ifs};
            serialization::ApplicationRepr app_repr{};
            ia >> app_repr;
            app_repr.Restore(app);
        } catch(const std::exception& e) {
            logging::LOG_INFO({{"what", e.what()}}, "Exception during deserialization");
            throw;
        }
    }
}

}  // namespace serialization
