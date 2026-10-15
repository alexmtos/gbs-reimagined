#include "net/web.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "core/settings.gen.h"
#include "net/ui_bundle.gen.h"
#include "version.h"

namespace gbsr::net {

namespace {

const char* kCollected[] = {"Origin", "Sec-Fetch-Site", "If-None-Match", "Host"};
// One profile import is about 0.6 KB of JSON (bench M2, backup run); anything far larger is not
// a profile and would only cost heap.
constexpr unsigned kMaxProfileBody = 4096;

// Streams an API answer as HTTP chunks: the status line goes out when the API announces it.
class ChunkSink : public core::Sink {
public:
    explicit ChunkSink(ESP8266WebServer& server) : server_(server) {}
    ~ChunkSink() override {
        if (started_) {
            server_.chunkedResponseFinalize();
        }
    }
    void status(int code) override {
        server_.sendHeader(F("Cache-Control"), F("no-store"));
        server_.chunkedResponseModeStart(code, "application/json");
        started_ = true;
    }
    void write(const char* data, size_t len) override { server_.sendContent(data, len); }

private:
    ESP8266WebServer& server_;
    bool started_ = false;
};

void formatIp(uint32_t ip, char* out, size_t cap) {
    snprintf(out, cap, "%u.%u.%u.%u", ip & 0xFF, ip >> 8 & 0xFF, ip >> 16 & 0xFF, ip >> 24);
}

} // namespace

void Web::begin() {
    server_.collectHeaders(kCollected, sizeof kCollected / sizeof kCollected[0]);
    server_.on(F("/"), HTTP_GET, [this] { handleUi(); });
    server_.on(F("/api/status"), HTTP_GET, [this] { handleStatus(); });
    server_.on(F("/api/settings"), [this] { handleSettings(); });
    server_.on(F("/api/command"), HTTP_POST, [this] { handleCommand(); });
    server_.on(F("/api/wifi"), [this] { handleWifi(); });
    server_.on(F("/api/wifi/scan"), HTTP_GET, [this] { handleScan(); });
    server_.on(F("/api/profiles"), HTTP_GET, [this] { handleProfiles(); });
    server_.on(F("/api/profile"), [this] { handleProfile(); });
    server_.on(F("/api/svs"), HTTP_POST, [this] { handleSwitcher(); });
    server_.on(F("/api/backup"), HTTP_GET, [this] { handleBackup(); });
    server_.on(F("/api/levels"), HTTP_POST, [this] { handleLevels(); });
    server_.onNotFound([this] { handleNotFound(); });
    server_.begin();
}

void Web::tick() {
    server_.handleClient();
}

void Web::sendJson(int code, const char* body) {
    server_.sendHeader(F("Cache-Control"), F("no-store"));
    server_.send(code, F("application/json"), body);
}

// Requests from other sites are refused: the API has no authentication, like most devices on a
// home network, so a page elsewhere must not be able to drive it (architecture.md, "HTTP API").
bool Web::sameOrigin() {
    if (server_.header(F("Sec-Fetch-Site")) == F("cross-site")) {
        return false;
    }
    const String origin = server_.header(F("Origin"));
    if (origin.length() == 0) {
        return true; // not a browser: curl, scripts, switchers
    }
    return origin == String(F("http://")) + server_.header(F("Host"));
}

void Web::handleUi() {
    if (server_.header(F("If-None-Match")) == ui::kEtag) {
        server_.send(304);
        return;
    }
    server_.sendHeader(F("Content-Encoding"), F("gzip"));
    server_.sendHeader(F("ETag"), ui::kEtag);
    server_.sendHeader(F("Cache-Control"), F("no-cache"));
    server_.setContentLength(ui::kSize);
    server_.send(200, F("text/html; charset=utf-8"), emptyString);
    // The body is written here rather than with send_P: the core streams flash data through a small
    // buffer and gives up at the first partial write, which the low-memory TCP stack produces on a
    // 20 KB page (bench 2026-10-13: browsers saw ERR_CONTENT_LENGTH_MISMATCH). Partial writes are
    // resumed; only ten seconds without progress abandon the client.
    WiFiClient client = server_.client();
    uint8_t block[512];
    uint32_t sent = 0;
    uint32_t progressMs = millis();
    while (sent < ui::kSize && client.connected()) {
        const uint32_t n = ui::kSize - sent < sizeof block ? ui::kSize - sent : sizeof block;
        memcpy_P(block, ui::kGzip + sent, n);
        uint32_t done = 0;
        while (done < n) {
            const size_t w = client.write(block + done, n - done);
            if (w > 0) {
                done += w;
                progressMs = millis();
            } else if (millis() - progressMs > 10'000 || !client.connected()) {
                return;
            } else {
                delay(1); // let the TCP stack take the acknowledgements
            }
        }
        sent += n;
    }
}

void Web::handleStatus() {
    char buf[768];
    if (core::statusJson(status_, GBSR_VERSION, settings::kSchemaHash, buf, sizeof buf) == 0) {
        sendJson(500, "{\"error\":\"status too large\"}");
        return;
    }
    sendJson(200, buf);
}

void Web::handleSettings() {
    ChunkSink sink(server_);
    if (server_.method() == HTTP_GET) {
        core::getSettings(ctl_, millis(), sink);
    } else if (server_.method() == HTTP_POST && sameOrigin()) {
        const String& body = server_.arg(F("plain"));
        core::postSettings(ctl_, body.c_str(), body.length(), millis(), sink);
    } else {
        sendJson(403, "{\"error\":\"refused\"}");
    }
}

void Web::handleCommand() {
    if (!sameOrigin()) {
        sendJson(403, "{\"error\":\"refused\"}");
        return;
    }
    core::Command ran = core::Command::None;
    {
        ChunkSink sink(server_);
        const String& body = server_.arg(F("plain"));
        core::postCommand(ctl_, body.c_str(), body.length(), millis(), sink, ran);
    }
    if (ran == core::Command::Reboot) {
        rebootRequested_ = true;
    }
}

void Web::handleProfiles() {
    ChunkSink sink(server_);
    core::getProfiles(ctl_, sink);
}

void Web::handleProfile() {
    if (server_.method() == HTTP_GET) {
        ChunkSink sink(server_);
        const long slot = server_.arg(F("slot")).toInt();
        core::getProfile(ctl_, slot >= 1 && slot <= core::kProfileSlots ? slot : 0, sink);
    } else if (server_.method() == HTTP_POST && sameOrigin()) {
        const String& body = server_.arg(F("plain"));
        if (body.length() > kMaxProfileBody) {
            sendJson(413, "{\"ok\":false,\"reason\":\"tooLarge\"}");
            return;
        }
        ChunkSink sink(server_);
        core::postProfile(ctl_, body.c_str(), body.length(), millis(), sink);
    } else {
        sendJson(403, "{\"error\":\"refused\"}");
    }
}

void Web::handleSwitcher() {
    if (!sameOrigin()) {
        sendJson(403, "{\"error\":\"refused\"}");
        return;
    }
    ChunkSink sink(server_);
    const String& body = server_.arg(F("plain"));
    core::postSwitcher(ctl_, body.c_str(), body.length(), millis(), sink);
}

void Web::handleBackup() {
    server_.sendHeader(F("Content-Disposition"), F("attachment; filename=\"gbs-backup.json\""));
    ChunkSink sink(server_);
    core::getBackup(ctl_, GBSR_VERSION, sink);
}

void Web::handleLevels() {
    if (!sameOrigin()) {
        sendJson(403, "{\"error\":\"refused\"}");
        return;
    }
    ChunkSink sink(server_);
    const String& body = server_.arg(F("plain"));
    core::postLevels(ctl_, body.c_str(), body.length(), sink);
}

void Web::handleWifi() {
    if (server_.method() == HTTP_POST) {
        if (!sameOrigin()) {
            sendJson(403, "{\"error\":\"refused\"}");
            return;
        }
        // {"ssid": ..., "password": ...}: small enough for the request's own argument parser.
        JsonDocument doc;
        const String& body = server_.arg(F("plain"));
        if (deserializeJson(doc, body) != DeserializationError::Ok ||
            !doc["ssid"].is<const char*>()) {
            sendJson(400, "{\"ok\":false,\"reason\":\"expectedSsid\"}");
            return;
        }
        const char* password = doc["password"] | "";
        if (wifi_.join(doc["ssid"], password, millis())) {
            sendJson(200, "{\"ok\":true}");
        } else {
            sendJson(400, "{\"ok\":false,\"reason\":\"invalid\"}");
        }
        return;
    }
    static const char* const kMode[] = {"joining", "joined", "setup"};
    char ip[16];
    char apIp[16];
    formatIp(wifi_.stationIp(), ip, sizeof ip);
    formatIp(wifi_.apIp(), apIp, sizeof apIp);
    ChunkSink sink(server_);
    sink.status(200);
    core::JsonOut j(sink);
    j.raw("{\"mode\":").str(kMode[static_cast<uint8_t>(wifi_.mode())]);
    j.raw(",\"ssid\":").str(wifi_.ssid()).raw(",\"ip\":").str(ip);
    j.raw(",\"rssi\":").num(wifi_.rssi()).raw(",\"hostname\":").str("gbs.local");
    static const char* const kFailure[] = {"", "refused", "notFound", "noAnswer"};
    j.raw(",\"lastFailure\":").str(kFailure[static_cast<uint8_t>(wifi_.lastFailure())]);
    j.raw(",\"ap\":{\"active\":").raw(wifi_.apActive() ? "true" : "false");
    j.raw(",\"ssid\":").str(wifi_.apSsid()).raw(",\"ip\":").str(apIp).raw("}}");
}

void Web::handleScan() {
    const int n = wifi_.scan();
    if (n < 0) {
        sendJson(200, "{\"scanning\":true}");
        return;
    }
    ChunkSink sink(server_);
    sink.status(200);
    core::JsonOut j(sink);
    j.raw("{\"scanning\":false,\"networks\":[");
    // Strongest first, each name once, at most 20.
    bool used[64] = {};
    int listed = 0;
    for (; listed < 20; ++listed) {
        int best = -1;
        for (int i = 0; i < n && i < 64; ++i) {
            if (!used[i] && WiFi.SSID(i).length() != 0 &&
                (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        for (int i = 0; i < n && i < 64; ++i) {
            used[i] = used[i] || WiFi.SSID(i) == WiFi.SSID(best);
        }
        j.raw(listed == 0 ? "{\"ssid\":" : ",{\"ssid\":").str(WiFi.SSID(best).c_str());
        j.raw(",\"rssi\":").num(WiFi.RSSI(best));
        j.raw(",\"secure\":").raw(WiFi.encryptionType(best) == ENC_TYPE_NONE ? "false}" : "true}");
    }
    j.raw("]}");
}

void Web::handleNotFound() {
    // With the setup access point up, any address leads to the UI: phones probe a few well-known
    // pages and show the portal when the answer is a redirect.
    if (wifi_.apActive()) {
        const String host = server_.header(F("Host"));
        char apIp[16];
        formatIp(wifi_.apIp(), apIp, sizeof apIp);
        if (host != apIp && host != F("gbs.local") && host != F("gbs")) {
            server_.sendHeader(F("Location"), String(F("http://")) + apIp + '/');
            server_.send(302);
            return;
        }
    }
    sendJson(404, "{\"error\":\"not found\"}");
}

} // namespace gbsr::net
