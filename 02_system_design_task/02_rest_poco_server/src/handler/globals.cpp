#include "request_counter.h"
#include "auth_config.h"

namespace handlers {

// Определение глобальных переменных
Poco::Prometheus::Counter* g_httpRequests = nullptr;
Poco::Prometheus::Counter* g_httpErrors = nullptr;
Poco::Prometheus::Histogram* g_httpDuration = nullptr;
std::string g_jwtSecret;

} // namespace handlers