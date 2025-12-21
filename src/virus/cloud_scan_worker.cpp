#include "virus/cloud_scan_worker.h"
#include "virus/cloud_scanner.h"
#include "monitoring/metrics.h"

CloudScanWorker& CloudScanWorker::instance() {
    static CloudScanWorker w;
    return w;
}

void CloudScanWorker::submit(const QueueMessage& msg) {
    Metrics::instance().inc("cloud_scan_submitted_total");

    // fire-and-forget with full context
    CloudScanner::instance().scanAsync(msg);
}
