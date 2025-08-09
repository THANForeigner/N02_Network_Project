#include "gmail.h"
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

  // --- FIX: Point to the CA certificate bundle ---
  // curl_easy_setopt(c, CURLOPT_CAINFO, "../cacert.pem");

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

// Helper function to base64-encode a file (used for attachments)
static std::string encode_file(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    std::cerr << "Error opening file: " << file_path << std::endl;
    return "";
  }

  std::string file_data((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

  return b64url(file_data);
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
  std::chrono::steady_clock::time_point expire_at;

  bool hasValidRefreshToken() const { return !refresh_token.empty(); }
  bool isAccessTokenExpired() const {
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
  std::ofstream("../token.json") << tok.dump().dump(2);
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
  // --- FIX: Point to the CA certificate bundle ---
  // curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
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
bool send_email_with_attachment(const std::string &bearer_token,
                                const std::string &to,
                                const std::string &subject,
                                const std::string &bodyText,
                                const std::string &file_path) {
  // 1. Read the file and base64 encode it
  std::string encoded_file = encode_file(file_path);
  if (encoded_file.empty()) {
    return false;
  }

  // 2. Prepare MIME message with attachment
  std::string mime = "To: " + to +
                     "\r\n"
                     "Subject: " +
                     subject +
                     "\r\n"
                     "Content-Type: multipart/mixed; boundary=\"boundary\"\r\n"
                     "\r\n"
                     "--boundary\r\n"
                     "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
                     "\r\n" +
                     bodyText +
                     "\r\n"
                     "--boundary\r\n"
                     "Content-Type: application/octet-stream\r\n"
                     "Content-Transfer-Encoding: base64\r\n"
                     "Content-Disposition: attachment; filename=\"" +
                     file_path +
                     "\"\r\n"
                     "\r\n" +
                     encoded_file +
                     "\r\n"
                     "--boundary--";

  // 3. Send the email
  std::string payload = "{\"raw\":\"" + b64url(mime) + "\"}";

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
  // --- FIX: Point to the CA certificate bundle ---
  //  curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
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
  for (char &c : s) // convert URL-safe chars -> standard chars
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  while (s.size() % 4)
    s.push_back('='); // add padding if missing

  static const int T[256] = {
      // reverse lookup table
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
// Helper function to get the HTTP response code from a curl handle
static long get_http_code(CURL *curl) {
  long http_code = 0;
  // Use CURLINFO_RESPONSE_CODE for the primary HTTP status
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  return http_code;
}

// "Bulletproof" version of the function for debugging
void mark_email_as_read(const std::string &bearer_token,
                        const std::string &msgId) {
  if (msgId.empty()) {
    return;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "[FATAL] Failed to initialize curl." << std::endl;
    return;
  }

  std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" +
                    msgId + "/modify";
  std::string json_payload = R"({"removeLabelIds": ["UNREAD"]})";
  std::string response_string; // To capture the server's error message

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + bearer_token).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

  // CRITICAL: Ensure SSL verification is enabled
  // curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");

  CURLcode res = curl_easy_perform(curl);
  long http_code = get_http_code(curl);

  if (res != CURLE_OK) {
    // This is a curl-level error (e.g., can't connect, SSL problem)
    std::cerr << "[ERROR] curl_easy_perform() failed: "
              << curl_easy_strerror(res) << std::endl;
  } else if (http_code >= 300) {
    // The API request was sent, but Google returned an error
    std::cerr << "[ERROR] Gmail API returned HTTP " << http_code << "."
              << std::endl;
    std::cerr << "[ERROR] Server Response: " << response_string << std::endl;
  } else {
    // Success!
    std::cout << "[SUCCESS] Gmail API returned HTTP " << http_code
              << ". Message marked as read." << std::endl;
  }

  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
}

// --- Reads the single latest unread email ---
bool read_latest_unread_email(const std::string &bearer_token,
                              std::string &mailhead, std::string &mailBody,
                              std::string &receiver) {
  std::string listResp;
  std::string msgId;

  // 1) Get the ID of the single latest unread message
  {
    CURL *c = curl_easy_init();
    if (!c)
      return false;

    struct curl_slist *h = nullptr;
    h = curl_slist_append(h, ("Authorization: Bearer " + bearer_token).c_str());

    // THIS IS THE KEY: "maxResults=1" ensures we only get one email ID
    curl_easy_setopt(c, CURLOPT_URL,
                     "https://gmail.googleapis.com/gmail/v1/users/me/"
                     "messages?maxResults=1&q=in:inbox%20is:unread");

    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &listResp);

    CURLcode res = curl_easy_perform(c);
    curl_slist_free_all(h);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) {
      std::cerr << "Failed to list messages: " << curl_easy_strerror(res)
                << std::endl;
      return false;
    }
  }

  try {
    auto jList = nlohmann::json::parse(listResp);
    // If "messages" is not found or is empty, there are no new emails.
    if (!jList.contains("messages") || jList["messages"].empty()) {
      // This is normal behavior when there are no unread emails.
      return false;
    }
    msgId = jList["messages"][0]["id"];
  } catch (const nlohmann::json::parse_error &e) {
    std::cerr << "JSON parse error (list messages): " << e.what() << std::endl;
    return false;
  }

  // If we reach here, we have exactly one message ID to process.

  // 2) Get the full content for that one message
  std::string msgResp;
  {
    CURL *c = curl_easy_init();
    if (!c)
      return false;

    std::string url =
        "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + msgId;
    struct curl_slist *h = nullptr;
    h = curl_slist_append(h, ("Authorization: Bearer " + bearer_token).c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &msgResp);

    CURLcode res = curl_easy_perform(c);
    curl_slist_free_all(h);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) {
      std::cerr << "Failed to get message details: " << curl_easy_strerror(res)
                << std::endl;
      return false;
    }
  }

  // 3) Extract data and body from the message
  try {
    auto jMsg = nlohmann::json::parse(msgResp);
    std::string subject, from_address;
    for (const auto &h : jMsg["payload"]["headers"]) {
      if (h["name"] == "Subject")
        subject = h["value"];
      else if (h["name"] == "From")
        from_address = h["value"];
    }

    mailhead = subject;
    receiver = from_address;

    std::string data;
    const auto &payload = jMsg["payload"];
    if (payload.contains("parts")) {
      for (const auto &p : payload["parts"]) {
        if (p["mimeType"] == "text/plain" && p.contains("body") &&
            p["body"].contains("data")) {
          data = p["body"]["data"];
          break;
        }
      }
    } else if (payload.contains("body") && payload["body"].contains("data")) {
      data = payload["body"]["data"];
    }

    if (data.empty()) {
      std::cerr << "No plain text part found in the email.\n";
      return false;
    }

    mailBody = decode_b64url(data); // Assumes you have this function

  } catch (const nlohmann::json::parse_error &e) {
    std::cerr << "JSON parse error (message details): " << e.what()
              << std::endl;
    return false;
  }

  // 4) Mark this specific email as read so we don't get it again
  mark_email_as_read(bearer_token, msgId);

  return true;
}

GmailClient::GmailClient() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  std::ifstream fs("../client_secret.json");
  if (!fs) {
    throw std::runtime_error("FATAL: client_secret.json not found.");
  }
  try {
    json secret = json::parse(fs);
    client_id_ = secret["installed"]["client_id"];
    client_secret_ = secret["installed"]["client_secret"];
  } catch (const json::exception &e) {
    throw std::runtime_error("FATAL: Could not parse client_secret.json: " +
                             std::string(e.what()));
  }

  token_box_ = std::make_unique<TokenBox>();
  std::ifstream ft("../token.json");
  if (ft) {
    try {
      *token_box_ = TokenBox::load(json::parse(ft));
      std::cout << "🔑 Token loaded from token.json\n";
    } catch (...) {
      std::cerr
          << "⚠️ Could not parse token.json. A new login may be required.\n";
    }
  } else {
    RunInteractiveLogin();
  }
}

GmailClient::~GmailClient() { curl_global_cleanup(); }

bool GmailClient::ensureValidToken() {
  if (!token_box_->isAccessTokenExpired()) {
    return true; // Token is fresh, nothing to do.
  }

  std::cout << "Access token has expired.\n";
  if (!token_box_->hasValidRefreshToken()) {
    std::cerr
        << "❌ No refresh token available. Please run interactive login.\n";
    return RunInterativeLogin();
  }

  std::cout << "Attempting to refresh token...\n";
  return refresh(*token_box_, client_id_, client_secret_);
}

bool GmailClient::RunInteractiveLogin() {
  const std::string redirect = "urn:ietf:wg:oauth:2.0:oob";
  // CORRECTED - FULL SCOPE
  const std::string scope = "https://www.googleapis.com/auth/gmail.modify "
                            "https://www.googleapis.com/auth/gmail.readonly "
                            "https://www.googleapis.com/auth/gmail.send";
  std::string auth_url =
      "https://accounts.google.com/o/oauth2/v2/auth?response_type=code" +
      std::string("&client_id=") + urlencode(client_id_) +
      "&redirect_uri=" + urlencode(redirect) + "&scope=" + urlencode(scope) +
      "&access_type=offline&prompt=consent";

  if (open_browser(auth_url))
    std::cout << "🌐 Browser opened. Please sign in and copy the authorization "
                 "code.\n";
  else
    std::cout << "⚠️ Could not open browser. Please open this URL manually:\n"
              << auth_url << "\n";

  std::string code;
  std::cout << "Paste code here: ";
  std::getline(std::cin, code);
  if (code.empty())
    return false;

  std::string body =
      "code=" + urlencode(code) + "&client_id=" + urlencode(client_id_) +
      "&client_secret=" + urlencode(client_secret_) +
      "&redirect_uri=" + urlencode(redirect) + "&grant_type=authorization_code";
  try {
    auto j = http_post("https://oauth2.googleapis.com/token", body);
    if (!j.contains("access_token")) {
      std::cerr << "❌ Authorization failed: " << j.dump(2) << "\n";
      return false;
    }
    token_box_->access_token = j["access_token"];
    if (j.contains("refresh_token")) { // This is crucial
      token_box_->refresh_token = j["refresh_token"];
    }
    token_box_->expire_at =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(j["expires_in"].get<int>() - 60);

    std::ofstream("../token.json") << token_box_->dump().dump(2);
    std::cout << "✅ Tokens saved successfully to token.json\n";
    return true;
  } catch (const std::exception &e) {
    std::cerr << "❌ Error during token exchange: " << e.what() << "\n";
    return false;
  }
}

bool GmailClient::SendEmail(const std::string &to, const std::string &subject,
                            const std::string &body) {
  if (!ensureValidToken()) {
    return false;
  }
  return send_email(token_box_->access_token, to, subject, body);
}

bool GmailClient::SendEmailAttachment(const std::string &to,
                                      const std::string &subject,
                                      const std::string &body,
                                      const std::string &attachment_path) {
  if (!ensureValidToken()) {
    return false;
  }
  return send_email_with_attachment(token_box_->access_token, to, subject, body,
                                    attachment_path);
}
bool GmailClient::GetLatestEmailBody(std::string &out_head,
                                     std::string &out_body,
                                     std::string &receiver) {
  if (!ensureValidToken()) {
    return false;
  }
  return read_latest_unread_email(token_box_->access_token, out_head, out_body,
                                  receiver);
}
