#pragma once
#ifndef GMAIL_CLIENT_H
#define GMAIL_CLIENT_H

#include "json.hpp"
#include <string>
#include <chrono>

struct TokenBox;

class GmailClient {
public:
    GmailClient();
    ~GmailClient();
    bool SendEmail(const std::string& to, const std::string& subject, const std::string& body);
    bool GetLatestEmailBody(std::string& out_body);
    bool RunInteractiveLogin();

private:
    bool ensureValidToken();
    std::string client_id_;
    std::string client_secret_;
    std::unique_ptr<TokenBox> token_box_; 
};

#endif 