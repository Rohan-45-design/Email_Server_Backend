#include "admin/admin_routes.h"
#include "admin/admin_auth.h"
#include "monitoring/metrics.h"
#include "monitoring/health.h"
#include "queue/mail_queue.h"
#include "spam/spam_engine.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

static std::string httpOK(const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + body;
}

std::string AdminRoutes::handleRequest(int sock) {
    char buf[1024]{};
    recv(sock, buf, sizeof(buf)-1, 0);
    std::string req(buf);
    if (!AdminAuth::authorize(req)) {
        return "HTTP/1.1 401 Unauthorized\r\n\r\n";
    }
    if (req.find("GET /admin/health") == 0) {
        auto h = Health::check();
        return httpOK(h.ok ? "OK" : "FAIL");
    }

    if (req.find("GET /admin/metrics") == 0) {
        return httpOK(Metrics::instance().renderPrometheus());
    }

    if (req.find("GET /admin/queue") == 0) {
        auto list = MailQueue::instance().list();
        std::ostringstream oss;
        for (auto& m : list)
            oss << m.id << "\n";
        return httpOK(oss.str());
    }

    return "HTTP/1.1 404 Not Found\r\n\r\n";
}
