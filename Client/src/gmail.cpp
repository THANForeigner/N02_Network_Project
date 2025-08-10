#include "gmail.h"
#include "json.hpp"    // JSON parsing / serialising
#include <chrono>      // token expiry timing
#include <curl/curl.h> // HTTP requests
#include <filesystem>
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
#ifdef WIN32
  curl_easy_setopt(c, CURLOPT_CAINFO, "../cacert.pem");
#endif
  CURLcode rc = curl_easy_perform(c);
  curl_easy_cleanup(c);

  if (rc != CURLE_OK)
    throw std::runtime_error("curl error: " +
                             std::string(curl_easy_strerror(rc)));

  return json::parse(resp);
}

// Minimal RFC‑4648 base64url encoder (no padding) — good enough for Gmail API
static std::string b64url(const std::string &in) {
  static const std::string b64_url_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789-_";

  std::string out;
  out.reserve(((in.length() / 3) + (in.length() % 3 > 0)) * 4);

  unsigned int i = 0;
  const char *bytes_to_encode = in.c_str();

  // Process 3-byte chunks
  while (i < in.length() - (in.length() % 3)) {
    unsigned char byte1 = bytes_to_encode[i++];
    unsigned char byte2 = bytes_to_encode[i++];
    unsigned char byte3 = bytes_to_encode[i++];

    out.push_back(b64_url_chars[(byte1 & 0xfc) >> 2]);
    out.push_back(b64_url_chars[((byte1 & 0x03) << 4) + ((byte2 & 0xf0) >> 4)]);
    out.push_back(b64_url_chars[((byte2 & 0x0f) << 2) + ((byte3 & 0xc0) >> 6)]);
    out.push_back(b64_url_chars[byte3 & 0x3f]);
  }

  // Handle the remaining 1 or 2 bytes
  if (in.length() % 3 == 1) {
    unsigned char byte1 = bytes_to_encode[i++];
    out.push_back(b64_url_chars[(byte1 & 0xfc) >> 2]);
    out.push_back(b64_url_chars[(byte1 & 0x03) << 4]);
  } else if (in.length() % 3 == 2) {
    unsigned char byte1 = bytes_to_encode[i++];
    unsigned char byte2 = bytes_to_encode[i++];
    out.push_back(b64_url_chars[(byte1 & 0xfc) >> 2]);
    out.push_back(b64_url_chars[((byte1 & 0x03) << 4) + ((byte2 & 0xf0) >> 4)]);
    out.push_back(b64_url_chars[(byte2 & 0x0f) << 2]);
  }

  return out;
}

static std::string base64_encode(const std::string &in) {
  std::string out;
  const std::string b64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(b64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (out.size() % 4) {
    out.push_back('=');
  }
  return out;
}
// Helper function to base64-encode a file (used for attachments)
static std::string base64url_encode(const std::string &in) {
  // 1. Get standard Base64
  std::string b64 = b64url(in);

  // 2. Replace characters for URL safety
  std::replace(b64.begin(), b64.end(), '+', '-');
  std::replace(b64.begin(), b64.end(), '/', '_');

  // 3. Remove padding
  size_t pad_pos = b64.find('=');
  if (pad_pos != std::string::npos) {
    b64.erase(pad_pos);
  }

  return b64;
}
static std::string encode_file(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    std::cerr << "Error opening file: " << file_path << std::endl;
    return "";
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  return base64_encode(ss.str());
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
#ifdef WIN32
  curl_easy_setopt(curl, CURLOPT_CAINFO, "../cacert.pem");
#endif
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
  // 1. Read the file and Base64 encode it (standard Base64)
  std::filesystem::path filepath(file_path);
  if (!std::filesystem::exists(filepath)) {
    std::cerr << "Error: File not found at " << file_path << "\n";
    return false;
  }
  std::string filename = filepath.filename().string();
  std::string encoded_file = encode_file(file_path);
  if (encoded_file.empty()) {
    std::cerr << "Error: Failed to read or encode file.\n";
    return false;
  }

  // 2. Prepare the full MIME message.
  // The MIME format is very strict. Pay close attention to the CRLF (\r\n) line
  // endings.
  std::string mime_message =
      "To: " + to +
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
      "\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "Content-Disposition: attachment; filename=\"" +
      filename +
      "\"\r\n"
      "\r\n" +
      encoded_file +
      "\r\n"
      "--boundary--";

  // 3. The Gmail API requires the entire raw message to be base64url encoded.
  std::string base64url_encoded_mime = base64url_encode(mime_message);
  if (base64url_encoded_mime.empty()) {
    std::cerr << "Error: Failed to base64url encode the MIME message.\n";
    return false;
  }

  // 4. Create the JSON payload.
  std::string json_payload = "{\"raw\":\"" + base64url_encoded_mime + "\"}";

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Error: curl_easy_init() failed.\n";
    return false;
  }

  std::string response_string;
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(
      headers, ("Authorization: Bearer " + bearer_token).c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // --- KEY FIX: Use CURLOPT_COPYPOSTFIELDS ---
  // This copies the payload data, preventing issues with the local
  // 'json_payload' string going out of scope or its memory becoming invalid.
  curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, json_payload.c_str());

  curl_easy_setopt(
      curl, CURLOPT_URL,
      "https://gmail.googleapis.com/gmail/v1/users/me/messages/send");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  // CURLOPT_POST is automatically set to 1 by CURLOPT_COPYPOSTFIELDS
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

  // For cross-platform compatibility, it's better to configure CA certs
  // properly. This line is fine for Windows if cacert.pem is in the parent
  // directory. On Linux, libcurl often finds system certs automatically.
#ifdef _WIN32
  curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
#endif

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
              << "\n";
    return false;
  }
  if (http_code < 200 || http_code >= 300) {
    std::cerr << "Gmail API Error: HTTP " << http_code
              << " Response: " << response_string << "\n";
    return false;
  }

  std::cout << "Email sent successfully! HTTP " << http_code << std::endl;
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
#ifdef WIN32
  curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
#endif
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
    // std::cout << "[SUCCESS] Gmail API returned HTTP " << http_code
    //          << ". Message marked as read." << std::endl;
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

#ifdef WIN32
    curl_easy_setopt(c, CURLOPT_CAINFO, "../cacert.pem");
#endif
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
#ifdef WIN32
    curl_easy_setopt(c, CURLOPT_CAINFO, "cacert.pem");
#endif
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
      // std::cerr << "No plain text part found in the email.\n";
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
      // std::cout << "🔑 Token loaded from token.json\n";
    } catch (...) {
      // std::cerr
      //     << "⚠️ Could not parse token.json. A new login may be required.\n";
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
    return RunInteractiveLogin();
  }

  std::cout << "Attempting to refresh token...\n";
  return refresh(*token_box_, client_id_, client_secret_);
}

bool GmailClient::RunInteractiveLogin() {
  const std::string redirect = "urn:ietf:wg:oauth:2.0:oob";
  // CORRECTED - FULL SCOPE
  const std::string scope = "https://www.googleapis.com/auth/gmail.modify "
                            "https://www.googleapis.com/auth/gmail.readonly "
                            "https://www.googleapis.com/auth/gmail.send "
                            "https://www.googleapis.com/auth/drive.file";
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

std::string
GmailClient::UploadToDriveAndGetShareableLink(const std::string &file_path) {
  if (!ensureValidToken()) {
    std::cerr << "Error: Cannot upload to Drive without a valid token.\n";
    return "";
  }

  // 1. Check if file exists and read its content
  std::ifstream file_stream(file_path, std::ios::binary);
  if (!file_stream) {
    std::cerr << "Error opening file for upload: " << file_path << std::endl;
    return "";
  }
  std::string file_content((std::istreambuf_iterator<char>(file_stream)),
                           std::istreambuf_iterator<char>());
  file_stream.close();

  // Extract just the filename from the path
  std::string filename = std::filesystem::path(file_path).filename().string();

  // 2. Perform a multipart upload to Google Drive
  std::cout << "Uploading " << filename << " to Google Drive..." << std::endl;

  CURL *curl = curl_easy_init();
  if (!curl)
    return "";

  std::string upload_response;
  std::string file_id;
  std::string web_link;

  // --- Part A: Upload the file ---
  {
    // The boundary is a random string that won't appear in the content.
    const std::string boundary = "----------BOUNDARY_STRING_12345";

    // Metadata part of the request (sets the filename on Google Drive)
    json metadata = {{"name", filename}};
    std::string request_body = "--" + boundary + "\r\n";
    request_body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    request_body += metadata.dump() + "\r\n";

    // File content part of the request
    request_body += "--" + boundary + "\r\n";
    request_body += "Content-Type: application/octet-stream\r\n\r\n";
    request_body += file_content + "\r\n";

    // Final boundary
    request_body += "--" + boundary + "--\r\n";

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(
        headers, ("Authorization: Bearer " + token_box_->access_token).c_str());
    headers = curl_slist_append(
        headers,
        ("Content-Type: multipart/related; boundary=" + boundary).c_str());

    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://www.googleapis.com/upload/drive/v3/"
                     "files?uploadType=multipart&fields=id,webViewLink");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body.length());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &upload_response);
#ifdef WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "../cacert.pem");
#endif

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (res != CURLE_OK || http_code >= 300) {
      std::cerr << "Google Drive upload failed. HTTP " << http_code
                << "\nResponse: " << upload_response << std::endl;
      curl_easy_cleanup(curl);
      return "";
    }

    try {
      json j = json::parse(upload_response);

      // First, check if the API returned an error object
      if (j.contains("error")) {
        std::string error_message = "Unknown API error";
        if (j["error"].contains("message")) {
          error_message = j["error"]["message"];
        }
        std::cerr << "Google Drive API returned an error: " << error_message
                  << std::endl;
        curl_easy_cleanup(curl);
        return "";
      }

      // Now, safely check for the fields we need
      if (!j.contains("id") || !j.contains("webViewLink")) {
        std::cerr
            << "Error: Drive API response is missing 'id' or 'webViewLink'."
            << std::endl;
        std::cerr << "Full Response: " << j.dump(2)
                  << std::endl; // Dump for debugging
        curl_easy_cleanup(curl);
        return "";
      }

      // If we get here, it's safe to access them
      file_id = j.at("id").get<std::string>();
      web_link = j.at("webViewLink").get<std::string>();
      std::cout << "File uploaded successfully. File ID: " << file_id
                << std::endl;
    } catch (const json::exception &e) {
      std::cerr << "Error parsing Drive upload response: " << e.what()
                << std::endl;
      std::cerr << "Raw Response: " << upload_response
                << std::endl; // Log the raw string
      curl_easy_cleanup(curl);
      return "";
    }
  }

  // --- Part B: Set permissions to make the file publicly readable ---
  if (!file_id.empty()) {
    std::cout << "Setting public permissions for the file..." << std::endl;
    curl_easy_reset(curl); // Reset curl handle for the next request

    std::string perm_url =
        "https://www.googleapis.com/drive/v3/files/" + file_id + "/permissions";
    json perm_payload = {{"role", "reader"}, {"type", "anyone"}};
    std::string perm_body = perm_payload.dump();
    std::string perm_response;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(
        headers, ("Authorization: Bearer " + token_box_->access_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, perm_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, perm_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &perm_response);
#ifdef WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "../cacert.pem");
#endif

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (res != CURLE_OK || http_code >= 300) {
      std::cerr << "Failed to set Drive permissions. HTTP " << http_code
                << "\nResponse: " << perm_response << std::endl;
      curl_easy_cleanup(curl);
      return "";
    }
    std::cout << "Permissions set successfully. The link is now public."
              << std::endl;
  }

  curl_easy_cleanup(curl);
  return web_link; // Return the shareable link
}

void GmailClient::SendVideoThroughEmail(const std::string &receiver,
                                        const std::string &videoPath) {
  if (!std::filesystem::exists(videoPath)) {
    std::cerr << "Error: Video file not found at " << videoPath << std::endl;
    SendEmail(receiver, "VIDEO_REQUEST_FAILED",
              "Sorry, the requested video could not be found.");
    return;
  }

  try {
    uintmax_t fileSize = std::filesystem::file_size(videoPath);
    std::string filename = std::filesystem::path(videoPath).filename().string();

    if (fileSize > MAX_ATTACHMENT_SIZE_BYTES) {
      std::cout << "Video is >25MB. Uploading to Google Drive..." << std::endl;

      // Use the new method to upload and get a link
      std::string shareableLink = UploadToDriveAndGetShareableLink(videoPath);

      if (!shareableLink.empty()) {
        std::string subject = "Link to Your Video: " + filename;
        std::string body =
            "The video you requested was too large to attach.\n\n"
            "You can view or download it from Google Drive using the link "
            "below:\n\n" +
            shareableLink;

        SendEmail(receiver, subject, body);
        std::cout << "Successfully sent email with Google Drive link."
                  << std::endl;
      } else {
        std::cerr
            << "Failed to upload to Google Drive. Sending failure notification."
            << std::endl;
        SendEmail(receiver, "VIDEO_REQUEST_FAILED",
                  "Sorry, there was an error processing your large video file. "
                  "The upload failed.");
      }
    } else {
      std::cout << "Video is small enough. Sending as a direct attachment..."
                << std::endl;
      std::string subject = "Your Video: " + filename;
      std::string body = "Please find the requested video file attached.";

      if (SendEmailAttachment(receiver, subject, body, videoPath)) {
        std::cout << "Successfully sent email with video attachment."
                  << std::endl;
      } else {
        std::cout << "Failed to send email with attachment." << std::endl;
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << std::endl;
  }
}
