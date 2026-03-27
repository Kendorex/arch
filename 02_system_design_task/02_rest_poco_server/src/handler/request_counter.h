#pragma once

#include <Poco/Prometheus/Counter.h>
#include <Poco/Prometheus/Histogram.h>

namespace handlers {

// Объявление глобальных переменных (extern)
extern Poco::Prometheus::Counter* g_httpRequests;
extern Poco::Prometheus::Counter* g_httpErrors;
extern Poco::Prometheus::Histogram* g_httpDuration;

} // namespace handlers