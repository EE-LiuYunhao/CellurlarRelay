
#include <curl/curl.h>
#include "sms.hpp"
#include "error.hpp"
#include <cstring>

namespace
{
    std::vector<unsigned char> hexToBytes(const std::string &hex)
    {
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            bytes.push_back(static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        }
        return bytes;
    }

    std::string decodeSemiOctet(const std::string &hex, int digits)
    {
        std::string result;
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            result.push_back(hex[i + 1]);
            if (hex[i] != 'F')
                result.push_back(hex[i]);
        }
        if ((int)result.size() > digits)
            result.resize(digits);
        return result;
    }

    std::string decodeGSM7(const std::vector<unsigned char> &data, int septetCount)
    {
        std::string out;
        int carryOver = 0;
        int carryBits = 0;
        for (int i = 0; i < septetCount; i++)
        {
            int byteIndex = (i * 7) / 8;
            int bitOffset = (i * 7) % 8;
            int val = (data[byteIndex] >> bitOffset) & 0x7F;
            if (bitOffset > 1 && byteIndex + 1 < (int)data.size())
            {
                val |= (data[byteIndex + 1] << (8 - bitOffset)) & 0x7F;
            }
            out.push_back((char)val);
        }
        return out;
    }

    std::string decodeUCS2(const std::vector<unsigned char> &data)
    {
        std::string out;
        for (size_t i = 0; i + 1 < data.size(); i += 2)
        {
            unsigned short int ch = (data[i] << 8) | data[i + 1];
            if (ch < 0x80)
            {
                out.push_back(static_cast<char>(ch));
            }
            else if (ch < 0x800)
            {
                out.push_back(0xC0 | (ch >> 6));
                out.push_back(0x80 | (ch & 0x3F));
            }
            else
            {
                out.push_back(0xE0 | (ch >> 12));
                out.push_back(0x80 | ((ch >> 6) & 0x3F));
                out.push_back(0x80 | (ch & 0x3F));
            }
        }
        return out;
    }

    std::string decodeTimestamp(const std::string &hex)
    {
        auto swap = [](char a, char b)
        { return std::string{b, a}; };
        std::string ts;
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            ts += hex[i + 1];
            ts += hex[i];
            if (i == 3 || i == 5)
                ts += '/';
            else if (i == 7)
                ts += ',';
            else if (i == 9 || i == 11)
                ts += ':';
        }
        return ts;
    }

    constexpr auto SENDER = "Home<peng_liu_home@currently.com>";
    constexpr auto RECEIVER = "Chuwei Peng<chwpeng@gmail.com>";

    constexpr auto SENDER_EMAIL_ADDRESS = "peng_liu_home@currently.com";
    constexpr auto SENDER_EMAIL_ADDRESS_BRACKET = "<peng_liu_home@currently.com>";
    constexpr auto SENDER_EMAIL_PASSWORD = "7nsaGdfvdNGK!FM";
    constexpr auto SENDER_EMAIL_SERVER = "smtps://smtp.mail.att.net:465";

    constexpr auto RECEIVER_EMAIL_BRACKET = "<chwpeng@gmail.com>";

    static size_t payload_reader(void *ptr, size_t size, size_t nmemb, void *userp)
    {
        const char **payload_text = (const char **)userp;
        if ((size == 0) || (nmemb == 0) || (*payload_text == NULL))
            return 0;

        size_t len = strlen(*payload_text);
        memcpy(ptr, *payload_text, len);
        *payload_text += len; // advance pointer
        return len;
    }
}

void SMS::sendEmail() const
{
    auto curl_client = curl_easy_init();
    if (curl_client == nullptr)
    {
        throw Utils::Error::EmailError(std::nullopt, " curl_easy_init gaves nullptr");
    }

    std::ostringstream email_formatter;
    email_formatter << "Date: " << "\r\n";
    email_formatter << "To: " << RECEIVER << "\r\n"
                    << "From: " << SENDER << "\r\n"
                    << "Subject: Recerived SMS from " << sender << "\r\n\r\n";

    auto lastChar = '\0';
    for (const auto &eachChar : content)
    {
        if (eachChar == '\n' && lastChar != '\r')
        {
            email_formatter << '\r' << eachChar;
        }
        else
        {
            email_formatter << eachChar;
        }
        lastChar = eachChar;
    }
    email_formatter << "\r\n";

    struct curl_slist *recipients = nullptr;

    // SMTP server (SSL on port 465)
    curl_easy_setopt(curl_client, CURLOPT_URL, SENDER_EMAIL_SERVER);

    // Authentication
    curl_easy_setopt(curl_client, CURLOPT_USERNAME, SENDER_EMAIL_ADDRESS);
    curl_easy_setopt(curl_client, CURLOPT_PASSWORD, SENDER_EMAIL_PASSWORD);

    // Sender and recipient
    curl_easy_setopt(curl_client, CURLOPT_MAIL_FROM, SENDER_EMAIL_ADDRESS_BRACKET);
    recipients = curl_slist_append(recipients, RECEIVER_EMAIL_BRACKET);
    curl_easy_setopt(curl_client, CURLOPT_MAIL_RCPT, recipients);

    // Message body
    curl_easy_setopt(curl_client, CURLOPT_READFUNCTION, payload_reader);
    curl_easy_setopt(curl_client, CURLOPT_READDATA, email_formatter.str().c_str());
    curl_easy_setopt(curl_client, CURLOPT_UPLOAD, 1L);

    // Perform the send
    auto res = curl_easy_perform(curl_client);
    curl_easy_cleanup(curl_client);

    if (res != CURLE_OK)
    {
        throw Utils::Error::EmailError(res, "failed to send " + email_formatter.str());
    }


}

SMS::SMS(const std::string &pdu)
{
    size_t idx = 0;

    // SMSC
    const auto smscLen = std::stoi(pdu.substr(idx, 2), nullptr, 16);
    if (smscLen > 0)
    {
        auto smscInfo = pdu.substr(idx + 2, smscLen * 2);
        smsc = decodeSemiOctet(smscInfo.substr(2), (smscLen - 1) * 2);
    }
    idx += 2 + smscLen * 2;

    // First octet of SMS-DELIVER
    idx += 2;

    // Sender number length
    const auto senderLen = std::stoi(pdu.substr(idx, 2), nullptr, 16);
    idx += 2;

    // Type-of-address
    idx += 2;

    // Sender number
    const auto senderBytes = (senderLen + 1) / 2 * 2;
    sender = decodeSemiOctet(pdu.substr(idx, senderBytes), senderLen);
    idx += senderBytes;

    // PID
    idx += 2;

    // DCS
    const auto dcs = std::stoi(pdu.substr(idx, 2), nullptr, 16);
    idx += 2;

    // Timestamp
    timestamp = decodeTimestamp(pdu.substr(idx, 14));
    idx += 14;

    // User data length
    const auto udl = std::stoi(pdu.substr(idx, 2), nullptr, 16);
    idx += 2;

    // User data
    std::vector<unsigned char> ud = hexToBytes(pdu.substr(idx));

    switch (dcs & 0x0C)
    {
    case 0x08:
        content = decodeUCS2(ud);
        break;
    case 0x00:
        content = decodeGSM7(ud, udl);
        break;
    default:
    {
        std::ostringstream errorReason;
        errorReason << " cannot recognize DCS field " << std::hex << dcs;
        throw Utils::Error::SMSParseError(pdu, errorReason.str());
    }
    }

    if ((dcs & 0x0C) == 0x08)
    {
        // UCS2
        content = decodeUCS2(ud);
    }
    else if ((dcs & 0x0C) == 0x00)
    {
        // Assume GSM 7-bit
        content = decodeGSM7(ud, udl);
    }
}

// todo: send email