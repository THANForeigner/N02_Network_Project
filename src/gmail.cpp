#include "json.hpp"    // JSON parsing / serialising
#include <chrono>      // token expiry timing
#include <curl/curl.h> // HTTP requests
#include <fstream>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h> // for Win32 ShellExecute fallback
#endif

using json = nlohmann::json;

// ───────────────────────── Helper routines ────────────────────────────

// Curl write‑callback → appends server response into std::string
static size_t write_cb(char *ptr, size_t size, size_t nm, void *userdata) {
  auto &buf = *static_cast<std::string *>(userdata);
  buf.append(ptr, size * nm);
  return size * nm;
}

// URL‑encode convenience wrapper around curl_easy_escape
static std::string urlencode(const std::string &s) {
  char *tmp = curl_easy_escape(nullptr, s.c_str(), 0);
  std::string out(tmp);
  curl_free(tmp);
  return out;
}

// Thin wrapper for HTTP POST that returns parsed JSON response
static json http_post(const std::string &url, const std::string &body) {
  CURL *c = curl_easy_init();
  std::string resp;

  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);

  CURLcode rc = curl_easy_perform(c);
  curl_easy_cleanup(c);

  if (rc != CURLE_OK)
    throw std::runtime_error("curl error: " +
                             std::string(curl_easy_strerror(rc)));

  return json::parse(resp);
}

// Minimal RFC‑4648 base64url encoder (no padding) — good enough for Gmail API
static std::string b64url(const std::string &in) {
  static const char *tbl =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(tbl[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
  return out; // Gmail accepts un‑padded output
}

// Portable "best‑effort" attempt to open default browser
static bool open_browser(const std::string &url) {
#ifdef _WIN32
  std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
  std::string cmd = "open \"" + url + "\"";
#else
  std::string cmd = "xdg-open \"" + url + "\"";
#endif
  return std::system(cmd.c_str()) == 0;
}

// ───────────────────────── Token container ────────────────────────────
// Stores access / refresh tokens and their absolute expiry time
// Provides JSON (de)serialise helpers so we can save to token.json
struct TokenBox {
  std::string access_token;
  std::string refresh_token;
  std::chrono::steady_clock::time_point expire_at; // access_token expiry

  bool expired() const {
    return access_token.empty() ||
           std::chrono::steady_clock::now() >= expire_at;
  }

  // convert to JSON for persistence
  json dump() const {
    return {{"access_token", access_token},
            {"refresh_token", refresh_token},
            {"expire_at", std::chrono::duration_cast<std::chrono::seconds>(
                              expire_at.time_since_epoch())
                              .count()}};
  }
  // construct from JSON (if token.json exists)
  static TokenBox load(const json &j) {
    TokenBox t;
    t.access_token = j.value("access_token", "");
    t.refresh_token = j.value("refresh_token", "");
    long long epoch = j.value("expire_at", 0LL);
    t.expire_at =
        std::chrono::steady_clock::time_point(std::chrono::seconds(epoch));
    return t;
  }
};

// Refreshes the access_token using the long‑lived refresh_token
// Returns true on success, false on fatal error (e.g. token revoked)
static bool refresh(TokenBox &tok, const std::string &client_id,
                    const std::string &client_secret) {
  if (tok.refresh_token.empty())
    return false; // nothing to refresh with

  std::string body = "client_id=" + urlencode(client_id) +
                     "&client_secret=" + urlencode(client_secret) +
                     "&refresh_token=" + urlencode(tok.refresh_token) +
                     "&grant_type=refresh_token";

  auto j = http_post("https://oauth2.googleapis.com/token", body);
  if (!j.contains("access_token")) {
    std::cerr << "refresh failed " << j.dump() << "\n";
    return false;
  }

  // Update in‑memory token
  tok.access_token = j["access_token"];
  tok.expire_at =
      std::chrono::steady_clock::now() +
      std::chrono::seconds(j["expires_in"].get<int>() - 60); // 60 s grace
  if (j.contains("refresh_token")) // Google may return new refresh_token
    tok.refresh_token = j["refresh_token"];

  // Persist to disk
  std::ofstream("token.json") << tok.dump().dump(2);
  std::cout << "🔄 token refreshed\n";
  return true;
}

// -----------------------------------------------------------------------------
// send_email() : convenience wrapper around
//   POST https://gmail.googleapis.com/gmail/v1/users/me/messages/send
//
// Parameters:
//   bearer_token  –  OAuth 2.0 access_token (string “ya29.…”) still valid
//   to            –  destination e‑mail address (e.g. "alice@example.com")
//   subject       –  message subject (UTF‑8 plain text)
//   bodyText      –  plain‑text body (UTF‑8)
//
// Returns:  true  on HTTP 200..299,  false otherwise.
// -----------------------------------------------------------------------------
bool send_email(const std::string &bearer_token, const std::string &to,
                const std::string &subject, const std::string &bodyText) {
  // ---- 1. build MIME string ------------------------------------------------
  std::string mime = "To: " + to +
                     "\r\n"
                     "Subject: " +
                     subject +
                     "\r\n"
                     "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
                     "\r\n" +
                     bodyText + "\r\n";

  // ---- 2. JSON payload with base64url‑encoded MIME -------------------------
  std::string payload = "{\"raw\":\"" + b64url(mime) + "\"}";

  // ---- 3. POST to Gmail API -----------------------------------------------
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;

  std::string resp;
  struct curl_slist *hdrs = nullptr;
  hdrs = curl_slist_append(hdrs,
                           ("Authorization: Bearer " + bearer_token).c_str());
  hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

  curl_easy_setopt(
      curl, CURLOPT_URL,
      "https://gmail.googleapis.com/gmail/v1/users/me/messages/send");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

  CURLcode rc = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK) {
    std::cerr << "curl error: " << curl_easy_strerror(rc) << "\n";
    return false;
  }
  if (httpCode < 200 || httpCode >= 300) {
    std::cerr << "Gmail returned HTTP " << httpCode << " → " << resp << "\n";
    return false;
  }
  // optional: parse resp JSON for "id"
  return true;
}
// -----------------------------------------------------------------------------
// read_latest_email() ‒ fetches the newest message, prints Subject + plain body
//
// Returns true if a message was fetched and decoded, false on error.
// -----------------------------------------------------------------------------
static std::string decode_b64url(std::string s) {
  for (char &c : s) // chuyển ký tự URL‑safe → chuẩn
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  while (s.size() % 4)
    s.push_back('='); // thêm padding nếu thiếu

  static const int T[256] = {
      // bảng tra ngược
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, 0,  -1, -1,
      -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
      -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
      /* rest -1 */
  };
  std::string out;
  out.reserve(s.size() * 3 / 4);

  int val = 0, valb = -8;
  for (unsigned char c : s) {
    if (T[c] == -1)
      break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}
bool read_latest_email(const std::string &bearer_token) {
  std::string listResp;
  { // 1) Lấy ID mới nhất
    CURL *c = curl_easy_init();
    struct curl_slist *h = nullptr;
    h = curl_slist_append(h, ("Authorization: Bearer " + bearer_token).c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_URL,
                     "https://gmail.googleapis.com/gmail/v1/users/me/"
                     "messages?maxResults=1&labelIds=INBOX");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &listResp);
    if (curl_easy_perform(c) != CURLE_OK) {
      curl_easy_cleanup(c);
      return false;
    }
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
  }
  auto jList = nlohmann::json::parse(listResp);
  if (jList["messages"].empty())
    return false;
  std::string msgId = jList["messages"][0]["id"];

  // 2) Lấy nội dung chi tiết
  std::string msgResp;
  {
    CURL *c = curl_easy_init();
    std::string url =
        "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + msgId +
        "?format=full";
    struct curl_slist *h = nullptr;
    h = curl_slist_append(h, ("Authorization: Bearer " + bearer_token).c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &msgResp);
    if (curl_easy_perform(c) != CURLE_OK) {
      curl_easy_cleanup(c);
      return false;
    }
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
  }
  auto jMsg = nlohmann::json::parse(msgResp);

  // 3) Trích Subject
  std::string subject;
  for (auto &h : jMsg["payload"]["headers"])
    if (h["name"] == "Subject") {
      subject = h["value"];
      break;
    }

  // 4) Lấy phần text/plain
  std::string data;
  auto &payload = jMsg["payload"];
  if (payload["body"]["size"].get<int>() > 0)
    data = payload["body"]["data"];
  else if (payload.contains("parts"))
    for (auto &p : payload["parts"])
      if (p["mimeType"] == "text/plain" && p["body"]["size"].get<int>() > 0) {
        data = p["body"]["data"];
        break;
      }
  if (data.empty()) {
    std::cerr << "No plain part\n";
    return false;
  }

  std::string body = decode_b64url(data);

  std::cout << "\n📨 Latest mail\nSubject: " << subject << "\nBody:\n"
            << body << "\n";
  return true;
}
// ─────────────────────────── Main program ────────────────────────────
int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  // STEP 1 — read client_secret.json
  // -------------------------------------------------
  std::ifstream fs("client_secret.json");
  if (!fs) {
    std::cerr << "❌ missing client_secret.json\n";
    return 1;
  }
  json secret;
  fs >> secret;
  auto inst = secret["installed"];
  std::string client_id = inst["client_id"];
  std::string client_secret = inst["client_secret"];

  // STEP 2 — try to load previously saved tokens
  // ------------------------------------
  TokenBox tok;
  if (std::ifstream ft("token.json"); ft) {
    json tj;
    ft >> tj;
    tok = TokenBox::load(tj);
  }

  // STEP 3 — if no valid access_token → run interactive OAuth flow
  // ------------------
  if (tok.expired()) {
    std::cout << "🔑 No valid access token – starting interactive login\n";

    // Google supports the special redirect URI below for CLI apps — code is
    // shown directly to the user instead of redirecting a browser.
    const std::string redirect = "urn:ietf:wg:oauth:2.0:oob";
    const std::string scope = "https://www.googleapis.com/auth/gmail.readonly "
                              "https://www.googleapis.com/auth/gmail.send";

    std::string auth_url =
        "https://accounts.google.com/o/oauth2/v2/auth?response_type=code" +
        std::string("&client_id=") + urlencode(client_id) +
        "&redirect_uri=" + urlencode(redirect) + "&scope=" + urlencode(scope) +
        "&access_type=offline&prompt=consent"; // ensures refresh_token on first
                                               // grant

    if (open_browser(auth_url))
      std::cout << "🌐 Browser opened – sign in and copy the code displayed.\n";
    else
      std::cout << "⚠️  Open this URL manually:\n" << auth_url << "\n";

    std::string code;
    std::cout << "Paste code: ";
    std::getline(std::cin, code);

    std::string body = "code=" + urlencode(code) +
                       "&client_id=" + urlencode(client_id) +
                       "&client_secret=" + urlencode(client_secret) +
                       "&redirect_uri=" + urlencode(redirect) +
                       "&grant_type=authorization_code";

    auto j = http_post("https://oauth2.googleapis.com/token", body);
    if (!j.contains("access_token")) {
      std::cerr << j.dump(2) << "\n";
      return 1;
    }

    tok.access_token = j["access_token"];
    tok.refresh_token = j.value("refresh_token", "");
    tok.expire_at = std::chrono::steady_clock::now() +
                    std::chrono::seconds(j["expires_in"].get<int>() - 60);

    // Save new token to disk for future runs
    std::ofstream("token.json") << tok.dump().dump(2);
    std::cout << "✅ token saved to token.json\n";
  } else {
    // token.json had a (possibly stale) access_token — try refreshing if needed
    refresh(tok, client_id, client_secret);
  }

  if (tok.expired() && !refresh(tok, client_id, client_secret)) {
    std::cerr << "❌ unable to refresh token\n";
    return 1;
  }
  if (send_email(tok.access_token, "phucbin0903@gmail.com",
                 "Test email from C++", "Hello from C++!")) {
    std::cout << "Email sent" << std::endl;
  } else {
    std::cerr << "Error sending email" << std::endl;
  }
  if (!read_latest_email(tok.access_token))
    std::cerr << "Không đọc được email mới nhất\n";
}
