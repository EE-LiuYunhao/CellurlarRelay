
#include "sms.hpp"
#include "error.hpp"

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