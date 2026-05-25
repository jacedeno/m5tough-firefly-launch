// Firefly Aerospace next-launch display for M5Stack M5Tough.
// Data: The Space Devs Launch Library 2 API (Firefly only).
// Two views (Mission Control / Hero) that auto-rotate and toggle on touch.

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <time.h>

// ---- Config ----
static const char* LL2_URL =
    "https://ll.thespacedevs.com/2.2.0/launch/upcoming/"
    "?search=Firefly&limit=1&mode=normal";
static const char* AP_NAME = "Firefly-Display-Setup";
static const uint32_t FETCH_INTERVAL_MS = 2UL * 60UL * 60UL * 1000UL; // 2h
static const uint32_t VIEW_SWITCH_MS = 8000;
static const uint32_t COUNTDOWN_TICK_MS = 1000;
static const int IMG_MAX_BYTES = 600000;

// ---- Colors (RGB565) ----
static constexpr uint16_t COL_BG = 0x0000;
static constexpr uint16_t COL_ORANGE = 0xFD20;
static constexpr uint16_t COL_WHITE = 0xFFFF;
static constexpr uint16_t COL_GREY = 0x8410;
static constexpr uint16_t COL_GREEN = 0x07E0;
static constexpr uint16_t COL_RED = 0xF800;
static constexpr uint16_t COL_YELLOW = 0xFFE0;

// ---- Launch data ----
struct LaunchData {
    String mission;
    String rocket;
    String statusName;
    String statusAbbrev;
    String pad;
    String location;
    String orbit;
    String image;
    time_t net;  // epoch UTC, 0 if unknown
    bool valid;
};
static LaunchData g_launch;

// ---- State ----
static int g_view = 0;  // 0 = Mission Control, 1 = Hero
static uint32_t g_lastSwitch = 0;
static uint32_t g_lastFetch = 0;
static uint32_t g_lastTick = 0;
static bool g_fullRedraw = true;
static uint8_t* g_img = nullptr;
static size_t g_imgLen = 0;

// ---- Helpers ----
static time_t parseISO8601(const String& s) {
    int Y, M, D, h, mi, se;
    if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ", &Y, &M, &D, &h, &mi, &se) != 6)
        return 0;
    struct tm tm = {};
    tm.tm_year = Y - 1900;
    tm.tm_mon = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    return mktime(&tm);  // TZ is UTC (configTime(0,0,...)), so == timegm
}

static String formatCountdown(time_t now, time_t net) {
    if (net == 0) return "T- --:--:--";
    long diff = (long)(net - now);
    bool past = diff < 0;
    if (past) diff = -diff;
    long days = diff / 86400;
    diff %= 86400;
    long hrs = diff / 3600;
    diff %= 3600;
    long mins = diff / 60;
    long secs = diff % 60;
    const char* sign = past ? "T+ " : "T- ";
    char buf[40];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%s%ldd %02ldh %02ldm", sign, days, hrs, mins);
    else if (hrs > 0)
        snprintf(buf, sizeof(buf), "%s%02ldh %02ldm %02lds", sign, hrs, mins, secs);
    else
        snprintf(buf, sizeof(buf), "%s%02ldm %02lds", sign, mins, secs);
    return String(buf);
}

static uint16_t statusColor(const String& ab) {
    if (ab == "Go" || ab == "Success") return COL_GREEN;
    if (ab == "Hold" || ab == "Failure") return COL_RED;
    if (ab == "TBD" || ab == "TBC") return COL_GREY;
    return COL_YELLOW;
}

static bool jpgSize(const uint8_t* d, size_t len, int* w, int* h) {
    if (len < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
    size_t i = 2;
    while (i + 9 < len) {
        if (d[i] != 0xFF) {
            i++;
            continue;
        }
        uint8_t m = d[i + 1];
        if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) ||
            (m >= 0xC9 && m <= 0xCB)) {
            *h = (d[i + 5] << 8) | d[i + 6];
            *w = (d[i + 7] << 8) | d[i + 8];
            return true;
        }
        size_t seg = (d[i + 2] << 8) | d[i + 3];
        i += 2 + seg;
    }
    return false;
}

// ---- Network ----
static void connectWiFi() {
    auto& d = M5.Display;
    d.fillScreen(COL_BG);
    d.setTextDatum(middle_center);
    d.setTextColor(COL_ORANGE, COL_BG);
    d.setTextSize(2);
    d.drawString("Connecting WiFi...", d.width() / 2, d.height() / 2 - 24);
    d.setTextSize(1);
    d.setTextColor(COL_GREY, COL_BG);
    d.drawString("No known network? Join AP:", d.width() / 2, d.height() / 2 + 16);
    d.setTextColor(COL_WHITE, COL_BG);
    d.drawString(AP_NAME, d.width() / 2, d.height() / 2 + 34);

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect(AP_NAME)) ESP.restart();
}

static void syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    struct tm t;
    for (int i = 0; i < 12 && !getLocalTime(&t, 500); i++) {
    }
}

static bool fetchLaunch() {
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    https.setUserAgent("M5Tough-FireflyDisplay/1.0 (github.com/jacedeno)");
    if (!https.begin(client, LL2_URL)) return false;
    int code = https.GET();
    if (code != 200) {
        https.end();
        return false;
    }

    JsonDocument filter;
    JsonObject r = filter["results"][0].to<JsonObject>();
    r["net"] = true;
    r["image"] = true;
    r["status"]["name"] = true;
    r["status"]["abbrev"] = true;
    r["rocket"]["configuration"]["full_name"] = true;
    r["mission"]["name"] = true;
    r["mission"]["orbit"]["name"] = true;
    r["pad"]["name"] = true;
    r["pad"]["location"]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, https.getStream(), DeserializationOption::Filter(filter));
    https.end();
    if (err) return false;

    JsonObject res = doc["results"][0];
    if (res.isNull()) return false;

    g_launch.mission = res["mission"]["name"] | "Unknown mission";
    g_launch.rocket = res["rocket"]["configuration"]["full_name"] | "Firefly Alpha";
    g_launch.statusName = res["status"]["name"] | "";
    g_launch.statusAbbrev = res["status"]["abbrev"] | "TBD";
    g_launch.pad = res["pad"]["name"] | "";
    g_launch.location = res["pad"]["location"]["name"] | "";
    g_launch.orbit = res["mission"]["orbit"]["name"] | "";
    g_launch.image = res["image"] | "";
    String net = res["net"] | "";
    g_launch.net = parseISO8601(net);
    g_launch.valid = true;
    return true;
}

static void downloadImage() {
    if (g_launch.image.length() == 0) return;
    if (g_img) {
        free(g_img);
        g_img = nullptr;
        g_imgLen = 0;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!https.begin(client, g_launch.image)) return;
    int code = https.GET();
    if (code != 200) {
        https.end();
        return;
    }
    int len = https.getSize();
    if (len <= 0 || len > IMG_MAX_BYTES) {
        https.end();
        return;
    }
    uint8_t* buf = (uint8_t*)ps_malloc(len);
    if (!buf) {
        https.end();
        return;
    }
    WiFiClient* stream = https.getStreamPtr();
    int got = 0;
    while (https.connected() && got < len) {
        size_t avail = stream->available();
        if (avail) {
            int n = stream->readBytes(buf + got, min(avail, (size_t)(len - got)));
            got += n;
        } else {
            delay(1);
        }
    }
    https.end();
    if (got == len) {
        g_img = buf;
        g_imgLen = len;
    } else {
        free(buf);
    }
}

// ---- Views ----
static void drawCountdownMC(time_t now) {
    auto& d = M5.Display;
    d.fillRect(0, 40, 320, 32, COL_BG);
    d.setTextDatum(middle_center);
    d.setTextColor(COL_ORANGE, COL_BG);
    d.setTextSize(3);
    d.drawString(formatCountdown(now, g_launch.net), 160, 56);
}

static void drawMissionControl(time_t now) {
    auto& d = M5.Display;
    d.fillScreen(COL_BG);
    d.fillRect(0, 0, 320, 26, COL_ORANGE);
    d.setTextColor(COL_BG, COL_ORANGE);
    d.setTextDatum(middle_left);
    d.setTextSize(2);
    d.drawString("FIREFLY", 8, 13);
    d.setTextDatum(middle_right);
    d.drawString(g_launch.statusAbbrev, 312, 13);

    drawCountdownMC(now);

    d.setTextSize(2);
    d.setTextDatum(middle_left);
    int y = 96;
    const int step = 28;
    auto row = [&](const char* label, const String& val) {
        d.setTextColor(COL_GREY, COL_BG);
        d.drawString(label, 8, y);
        d.setTextColor(COL_WHITE, COL_BG);
        d.drawString(val.length() ? val : String("-"), 100, y);
        y += step;
    };
    row("MISSION", g_launch.mission);
    row("ROCKET", g_launch.rocket);
    row("PAD", g_launch.pad);
    row("SITE", g_launch.location);
    row("ORBIT", g_launch.orbit);
}

static void drawCountdownHero(time_t now) {
    auto& d = M5.Display;
    d.fillRect(0, 206, 320, 28, COL_BG);
    d.setTextDatum(bottom_left);
    d.setTextColor(COL_ORANGE, COL_BG);
    d.setTextSize(3);
    d.drawString(formatCountdown(now, g_launch.net), 8, 234);
}

static void drawHero(time_t now) {
    auto& d = M5.Display;
    for (int yy = 0; yy < 240; yy++) {
        uint8_t r = map(yy, 0, 240, 35, 0);
        uint8_t g = map(yy, 0, 240, 12, 0);
        uint8_t b = map(yy, 0, 240, 45, 8);
        d.drawFastHLine(0, yy, 320, d.color565(r, g, b));
    }
    if (g_img && g_imgLen) {
        int w = 0, h = 0;
        if (jpgSize(g_img, g_imgLen, &w, &h) && w > 0) {
            float scale = 320.0f / w;
            d.drawJpg(g_img, g_imgLen, 0, 0, 320, (int)(h * scale), 0, 0, scale,
                      scale);
        } else {
            d.drawJpg(g_img, g_imgLen, 0, 0);
        }
    }
    d.fillRect(0, 150, 320, 90, COL_BG);
    d.setTextDatum(top_left);
    d.setTextColor(COL_WHITE, COL_BG);
    d.setTextSize(2);
    d.drawString(g_launch.mission, 8, 156);
    d.setTextColor(COL_GREY, COL_BG);
    d.setTextSize(1);
    d.drawString(g_launch.rocket + "  -  " + g_launch.location, 8, 184);
    drawCountdownHero(now);
}

static void render(bool full) {
    time_t now = time(nullptr);
    if (full) {
        if (g_view == 0)
            drawMissionControl(now);
        else
            drawHero(now);
    } else {
        if (g_view == 0)
            drawCountdownMC(now);
        else
            drawCountdownHero(now);
    }
}

// ---- Lifecycle ----
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setBrightness(180);

    // Hold the screen while powering on to wipe saved WiFi and re-run setup.
    M5.update();
    if (M5.Touch.getCount() > 0) {
        WiFiManager wm;
        wm.resetSettings();
        M5.Display.fillScreen(COL_BG);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(COL_RED, COL_BG);
        M5.Display.setTextSize(2);
        M5.Display.drawString("WiFi reset", 160, 120);
        delay(1200);
    }

    connectWiFi();
    syncTime();

    M5.Display.fillScreen(COL_BG);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COL_ORANGE, COL_BG);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Loading launch...", 160, 120);

    fetchLaunch();
    downloadImage();

    uint32_t t = millis();
    g_lastFetch = t;
    g_lastSwitch = t;
    g_lastTick = t;
    g_fullRedraw = true;
}

void loop() {
    M5.update();

    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) {
        g_view ^= 1;
        g_lastSwitch = millis();
        g_fullRedraw = true;
    }

    if (millis() - g_lastSwitch >= VIEW_SWITCH_MS) {
        g_view ^= 1;
        g_lastSwitch = millis();
        g_fullRedraw = true;
    }

    if (millis() - g_lastFetch >= FETCH_INTERVAL_MS) {
        if (fetchLaunch()) downloadImage();
        g_lastFetch = millis();
        g_fullRedraw = true;
    }

    bool tick = (millis() - g_lastTick >= COUNTDOWN_TICK_MS);
    if (tick) g_lastTick = millis();

    if (g_fullRedraw) {
        render(true);
        g_fullRedraw = false;
    } else if (tick) {
        render(false);
    }

    delay(10);
}
