#pragma once
#ifndef GMAIL_CLIENT_H
#define GMAIL_CLIENT_H

#include "json.hpp"
#include <chrono>
#include <string>

struct TokenBox;
const long long MAX_ATTACHMENT_SIZE_BYTES = 25 * 1024 * 1024;

class GmailClient {
public:
  GmailClient();
  ~GmailClient();
  bool SendEmail(const std::string &to, const std::string &subject,
                 const std::string &body);
  bool SendEmailAttachment(const std::string &to, const std::string &subject,
                           const std::string &body,
                           const std::string &attachment_path);
  bool GetLatestEmailBody(std::string &out_head,std::string &out_body, std::string &receiver);
  bool RunInteractiveLogin();
  std::string UploadToDriveAndGetShareableLink(const std::string &file_path);
  void SendVideoThroughEmail(const std::string& receiver, const std::string& videoPath);
private:
  bool ensureValidToken();
  std::string client_id_;
  std::string client_secret_;
  std::unique_ptr<TokenBox> token_box_;
};

#endif
