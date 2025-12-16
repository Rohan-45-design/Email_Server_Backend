#include "monitoring/health.h"
#include "monitoring/metrics.h"

HealthStatus Health::check() {
    // Simple baseline checks (extend later)
    HealthStatus s;
    s.ok = true;
    s.message = "OK";
    return s;
}
