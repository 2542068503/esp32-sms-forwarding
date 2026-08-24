#include "ddns.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "../logger/logger.h"

// Dynv6 动态域名配置 (请在此填写你自己的配置)
static const char* DYNV6_HOSTNAME = "your_domain.dynv6.net";
static const char* DYNV6_TOKEN    = "YOUR_DYNV6_TOKEN";

// 更新周期：10 分钟（毫秒）
constexpr unsigned long DDNS_UPDATE_INTERVAL_MS = 10 * 60 * 1000;

static String        s_lastIpv6       = "";
static bool          s_lastSuccess    = false;
static unsigned long s_lastUpdateTime = 0;
static unsigned long s_lastAttemptMs  = 0;

void Ddns::init() {
  s_lastIpv6 = "";
  s_lastSuccess = false;
  s_lastUpdateTime = 0;
  s_lastAttemptMs = 0;
  LOG("DDNS", "Dynv6 DDNS 模块已初始化: %s", DYNV6_HOSTNAME);
}

String Ddns::lastIpv6() {
  return s_lastIpv6;
}

bool Ddns::lastUpdateSuccess() {
  return s_lastSuccess;
}

unsigned long Ddns::lastUpdateTime() {
  return s_lastUpdateTime;
}

static void doUpdate(const String& ipv6) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("DDNS", "WiFi 未连接，跳过 DDNS 更新");
    return;
  }

  String url = "http://dynv6.com/api/update?hostname=";
  url += DYNV6_HOSTNAME;
  url += "&token=";
  url += DYNV6_TOKEN;
  if (ipv6.length() > 0 && !ipv6.equals("0000:0000:0000:0000:0000:0000:0000:0000") && !ipv6.equals("::")) {
    url += "&ipv6=";
    url += ipv6;
  }

  LOG("DDNS", "正在向 Dynv6 上报 IP... (IPv6: %s)", ipv6.c_str());

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(url)) {
    LOG("DDNS", "HTTP 连接初始化失败");
    s_lastSuccess = false;
    return;
  }

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    payload.trim();
    if (httpCode == 200) {
      LOG("DDNS", "✅ Dynv6 更新成功: %s", payload.c_str());
      s_lastSuccess = true;
      s_lastIpv6 = ipv6;
      s_lastUpdateTime = millis();
    } else {
      LOG("DDNS", "⚠️ Dynv6 更新返回 HTTP %d: %s", httpCode, payload.c_str());
      s_lastSuccess = false;
    }
  } else {
    LOG("DDNS", "❌ Dynv6 请求失败: %s", http.errorToString(httpCode).c_str());
    s_lastSuccess = false;
  }
  http.end();
}

void Ddns::forceUpdate() {
  String currentIpv6 = WiFi.globalIPv6().toString();
  if (currentIpv6.length() == 0 || currentIpv6.equals("::") || currentIpv6.equals("0000:0000:0000:0000:0000:0000:0000:0000")) {
    currentIpv6 = WiFi.localIPv6().toString();
  }
  doUpdate(currentIpv6);
  s_lastAttemptMs = millis();
}

void Ddns::tick() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String currentIpv6 = WiFi.globalIPv6().toString();
  if (currentIpv6.length() == 0 || currentIpv6.equals("::") || currentIpv6.equals("0000:0000:0000:0000:0000:0000:0000:0000")) {
    currentIpv6 = WiFi.localIPv6().toString();
  }

  unsigned long now = millis();

  // 条件1：首次获取到有效 IPv6 且尚未更新成功
  // 条件2：检测到 IPv6 地址发生变化
  // 条件3：到达 10 分钟定期刷新周期
  bool ipChanged = (currentIpv6.length() > 0 && !currentIpv6.equals(s_lastIpv6));
  bool timeExpired = (s_lastAttemptMs == 0 || now - s_lastAttemptMs >= DDNS_UPDATE_INTERVAL_MS);

  if (ipChanged || timeExpired) {
    s_lastAttemptMs = now;
    doUpdate(currentIpv6);
  }
}
