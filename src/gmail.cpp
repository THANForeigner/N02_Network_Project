
// gmail_oauth_refresh.cpp
#include "json.hpp"
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <string>

using json = nlohmann::json;

const std::string client_id =
    "727062445081-p7gev5sv3qhhu6do4kgm9im4r7rj0ksq.apps.googleusercontent.com";
const std::string client_secret = "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";
const std::string redirect_uri = "urn:ietf:wg:oauth:2.0:oob";

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *output) {
  output->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string requestNewToken(std::string &refresh_token_out,
                            std::string &access_token_out,
                            time_t &expires_at_out) {
  std::string auth_url =
      "https://accounts.google.com/o/oauth2/v2/auth?" +
      std::string("client_id=") + client_id + "&redirect_uri=" + redirect_uri +
      "&response_type=code&scope=https%3A%2F%2Fwww.googleapis.com%2Fauth%"
      "2Fgmail.readonly&access_type=offline&prompt=consent";

#ifdef _WIN32
  system(("start " + auth_url).c_str());
#elif __APPLE__
  system(("open \"" + auth_url + "\"").c_str());
#else
  system(("xdg-open \"" + auth_url + "\"").c_str());
#endif

  std::cout << "\n🔑 Paste authorization code here: ";
  std::string auth_code;
  std::cin >> auth_code;

  CURL *curl = curl_easy_init();
  std::string response;

  if (!curl)
    return "";

  std::string post_fields = "code=" + auth_code + "&client_id=" + client_id +
                            "&client_secret=" + client_secret +
                            "&redirect_uri=" + redirect_uri +
                            "&grant_type=authorization_code";

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    return "";

  auto j = json::parse(response);
  if (j.contains("access_token") && j.contains("refresh_token")) {
    access_token_out = j["access_token"];
    refresh_token_out = j["refresh_token"];
    expires_at_out = std::time(nullptr) + int(j["expires_in"]);

    json save = {{"access_token", access_token_out},
                 {"refresh_token", refresh_token_out},
                 {"expires_at", expires_at_out}};
    std::ofstream out("token.json");
    out << save.dump(2);
    out.close();
    return access_token_out;
  }

  std::cerr << "Response: " << response << "\n";
  return "";
}

std::string refreshAccessToken(const std::string &refresh_token,
                               const std::string &client_id,
                               const std::string &client_secret,
                               std::string &access_token_out,
                               time_t &expires_at_out) {
  CURL *curl = curl_easy_init();
  std::string response;

  if (!curl)
    return "";

  std::string post_fields =
      "client_id=" + client_id + "&client_secret=" + client_secret +
      "&refresh_token=" + refresh_token + "&grant_type=refresh_token";

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::cerr << "Failed to refresh token.\n";
    return "";
  }

  auto j = json::parse(response);
  if (j.contains("access_token")) {
    access_token_out = j["access_token"];
    expires_at_out = std::time(nullptr) + int(j["expires_in"]);
    return access_token_out;
  }

  std::cerr << "Response: " << response << "\n";
  return "";
}

std::string loadAccessToken(std::string &refresh_token,
                            const std::string &client_id,
                            const std::string &client_secret) {
  std::ifstream in("token.json");
  if (!in) {
    std::cout << "🔐 No token found. Starting authorization flow...\n";
    std::string access_token, new_refresh_token;
    time_t expires_at;
    access_token = requestNewToken(new_refresh_token, access_token, expires_at);
    refresh_token = new_refresh_token;
    return access_token;
  }

  json token_data;
  in >> token_data;
  in.close();

  refresh_token = token_data["refresh_token"];
  std::string access_token = token_data.value("access_token", "");
  time_t expires_at = token_data.value("expires_at", 0);

  time_t now = std::time(nullptr);
  if (access_token.empty() || now >= expires_at) {
    std::cout << "🔁 Access token expired. Refreshing...\n";
    std::string new_token;
    time_t new_expiry;
    new_token = refreshAccessToken(refresh_token, client_id, client_secret,
                                   new_token, new_expiry);

    if (!new_token.empty()) {
      json new_data = {{"refresh_token", refresh_token},
                       {"access_token", new_token},
                       {"expires_at", new_expiry}};
      std::ofstream out("token.json");
      out << new_data.dump(2);
      out.close();
      return new_token;
    }
    return "";
  }

  return access_token;
}

int main() {
  std::string refresh_token;
  std::string access_token =
      loadAccessToken(refresh_token, client_id, client_secret);

  if (access_token.empty()) {
    std::cerr << "❌ Failed to obtain access token.\n";
    return 1;
  }

  std::cout << "✅ Access token: " << access_token.substr(0, 25) << "...\n";
  return 0;
}
