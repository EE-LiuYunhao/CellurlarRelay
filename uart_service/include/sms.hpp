#ifndef SMS_HPP
#define SMS_HPP

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <tuple>
#include <mutex>
#include <memory>

#include "options.hpp"

class SMS
{
public:
    explicit SMS(const std::string &pdu);

    SMS(std::string sender, std::string content); // for plain-text debug

    SMS() = default;
    SMS(const SMS &other) = default;
    SMS(SMS &&other) = default;
    SMS &operator=(const SMS &other) = default;
    SMS &operator=(SMS &&other) = default;

    friend std::ostream& operator<<(std::ostream& os, const SMS& message);

    void send_email() const;

    typedef std::tuple<unsigned short, std::vector<std::string>> SegmentCountAndContents;


private:
    std::string smsc;
    std::string sender;
    std::string timestamp;
    std::string content;
    bool is_segment;
    unsigned short reference = 0;

    // for long SMS lookup
    static std::unordered_map<unsigned short, SegmentCountAndContents> ref_to_segments; 

    static std::once_flag config_init;
    
    static std::unique_ptr<Utils::Options::Email> email_config;
};

#endif
