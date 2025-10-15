#ifndef SMS_HPP
#define SMS_HPP

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

class SMS
{
public:
    explicit SMS(const std::string &pdu);
    SMS() = default;
    SMS(const SMS &other) = default;
    SMS(SMS &&other) = default;
    SMS &operator=(const SMS &other) = default;
    SMS &operator=(SMS &&other) = default;

    void sendEmail() const;

private:
    std::string smsc;
    std::string sender;
    std::string timestamp;
    std::string content;
};

#endif