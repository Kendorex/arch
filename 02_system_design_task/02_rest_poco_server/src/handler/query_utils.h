#pragma once

#include <Poco/URI.h>
#include <string>

namespace handlers {

// Вспомогательная функция для получения query параметра
inline std::string getQueryParameter(const Poco::URI& uri, const std::string& name) {
    const auto& params = uri.getQueryParameters();
    for (const auto& param : params) {
        if (param.first == name) {
            return param.second;
        }
    }
    return "";
}

} // namespace handlers