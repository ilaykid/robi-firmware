#ifndef ROBI_HTTP_PORTAL_H_
#define ROBI_HTTP_PORTAL_H_

#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <mutex>

class RobiHttpPortal {
public:
    static RobiHttpPortal& GetInstance();

    void Start();
    void Stop();

private:
    RobiHttpPortal() = default;
    void MdnsResponderLoop();

    std::mutex mutex_;
    httpd_handle_t server_ = nullptr;
    TaskHandle_t mdns_task_ = nullptr;
    std::atomic<bool> mdns_running_{false};
    int mdns_sock_ = -1;
};

#endif  // ROBI_HTTP_PORTAL_H_
