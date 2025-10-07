
#include <curl/curl.h>
#include <chrono>
#include <cstring>
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

    std::string decodeGSM7(const std::vector<unsigned char> &data, const unsigned int skip, const unsigned int septetCount)
    {
        std::string out;
        int carryOver = 0;
        int carryBits = 0;
        for (auto i = skip; i < septetCount; i++)
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

    std::string decodeUCS2(const std::vector<unsigned char> &data, const unsigned int skip)
    {
        std::string out;
        for (auto i = skip; i + 1 < data.size(); i += 2)
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

	struct EmailPayloadCarrier
	{
		const std::string& data; // pointer to the email body
		size_t pos;              // current position in the string
	};

    static size_t payload_reader(void *ptr, size_t size, size_t nmemb, void *userp)
    {
		auto* payload = static_cast<EmailPayloadCarrier *>(userp);
		size_t buffer_size = size * nmemb;

		if (payload->pos > payload->data.size())
			return 0; // no more data to send

		auto copy_len = payload->data.size() + 1 - payload->pos;
		copy_len = copy_len <= buffer_size ? copy_len : buffer_size;

		memcpy(ptr, payload->data.data() + payload->pos, copy_len);
		payload->pos += copy_len;

		return copy_len;
    }
}

std::unordered_map<unsigned short, SMS::SegmentCountAndContents> SMS::ref_to_segments; 
std::once_flag SMS::config_init;
std::unique_ptr<Utils::Options::Email> SMS::email_config = nullptr;

void SMS::send_email() const
{

	std::call_once(config_init, [this](){ email_config = std::make_unique<Utils::Options::Email>(); });

	if (is_segment)
	{
		if (auto segmentIdx = ref_to_segments.find(reference);
			segmentIdx == ref_to_segments.end() || std::get<1>(segmentIdx->second).size() != std::get<0>(segmentIdx->second))
		{
			std::cout << "Sending segmented SMS with ref = " << reference
					  << " but the segments is missing or not completed" << std::endl;
			return;
		}
	}
    auto curl_client = curl_easy_init();
    if (curl_client == nullptr)
    {
        throw Utils::Error::EmailError(std::nullopt, " curl_easy_init gaves nullptr");
    }

    auto last_char = '\0';
	std::string full_content;
	std::ostringstream sms_concat;
	if (is_segment)
	{
		auto node = ref_to_segments.extract(reference);
		if (!node.empty())
		{
			auto& all_seg = std::get<1>(node.mapped());
			for (const auto each_seg : all_seg) sms_concat << each_seg;
			full_content = sms_concat.str();
		}
	}

	auto content_selector = [this, &full_content]()
	{
		if (is_segment) return full_content;
		return content;
	};

	if (email_config == nullptr || !email_config->is_valid())
	{
		std::cout << "Email config is not valid: cannot send email, log only.\nComplete SMS is : "
				  << content_selector() << std::endl;
		curl_easy_cleanup(curl_client);
		return;
	}

    std::ostringstream email_formatter;

	auto now = std::chrono::system_clock::now();
    auto current_time = std::chrono::system_clock::to_time_t(now);


    email_formatter << "Date: " << std::put_time(std::localtime(&current_time), "%a, %d %b %Y %H:%M:%S %z") << "\r\n";
    email_formatter << "To: " << email_config->get_receiver() << "\r\n"
                    << "From: " << email_config->get_sender() << "\r\n"
                    << "Subject: Received SMS from " << sender
					<< "\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: 8bit\r\n\r\n";

	
    for (const auto &each_char : content_selector())
    {
        if (each_char == '\n' && last_char != '\r')
        {
            email_formatter << '\r' << each_char;
        }
        else
        {
            email_formatter << each_char;
        }
        last_char = each_char;
    }
    email_formatter << "\r\n";

	EmailPayloadCarrier email_body { email_formatter.str(), 0 };
	std::cout << "Email: " << email_body.data << std::endl;

    struct curl_slist *recipients = nullptr;

    // SMTP server (SSL on port 465)
    curl_easy_setopt(curl_client, CURLOPT_URL, email_config->get_server().c_str());
	curl_easy_setopt(curl_client, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

    // Authentication
    curl_easy_setopt(curl_client, CURLOPT_USERNAME, email_config->get_sender_email().c_str());
    curl_easy_setopt(curl_client, CURLOPT_PASSWORD, email_config->get_password().c_str());

    // Sender and recipient
    curl_easy_setopt(curl_client, CURLOPT_MAIL_FROM, email_config->get_sender_bracket().c_str());
    recipients = curl_slist_append(recipients, email_config->get_receiver_bracket().c_str());
    curl_easy_setopt(curl_client, CURLOPT_MAIL_RCPT, recipients);


    // Message body
    curl_easy_setopt(curl_client, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl_client, CURLOPT_READFUNCTION, payload_reader);
    curl_easy_setopt(curl_client, CURLOPT_READDATA, &email_body);

    // Perform the send
	// curl_easy_setopt(curl_client, CURLOPT_VERBOSE, 1L);
    auto res = curl_easy_perform(curl_client);
    curl_easy_cleanup(curl_client);
	curl_slist_free_all(recipients);

    if (res != CURLE_OK)
    {
        throw Utils::Error::EmailError(res, "failed to send " + email_formatter.str());
    }
}
    
std::ostream& operator<<(std::ostream& os, const SMS& message)
{
	os << "SMS:\n{\n\tsmsc: {" << message.smsc
	   << "},\n\tsender: {" << message.sender
	   << "},\n\ttimestamp: {" << message.timestamp
	   << "},\n\tsegment: {";
	if (message.is_segment)
	{
		os << "\n\t\treference: " << message.reference;
		if (auto segmentIdx = SMS::ref_to_segments.find(message.reference); segmentIdx != SMS::ref_to_segments.end())
		{
			os << "\n\t\tcount: " << std::get<1>(segmentIdx->second).size()
			   << ",\n\t\treceived: " << std::get<0>(segmentIdx->second)
			   << ",\n\t\tall: [";
			for (const auto each: std::get<1>(segmentIdx->second))
			{
				os << "\n\t\t\t{" << each << "},";
			}
			os << "\n\t\t]";
		}
	}
	os << "},\n\tcontent: {" << message.content
	   << "}\n}";
	return os;
}

SMS::SMS(std::string sender_, std::string content_): 
	sender(std::move(sender_)), content(std::move(content_)) {} 

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
	const auto firstOctet = std::stoi(pdu.substr(idx, 2), nullptr, 16);
	bool udhi = (firstOctet & 0x40) != 0; // indicate UDH exsits
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
	unsigned int skip = 0;
	int count = 0; // total segment count
	int index = 0; // index of the current segment

	if (udhi)
	{
		skip = static_cast<unsigned int>(ud[0]) + 1;
		if (ud[0] == 0) throw Utils::Error::SMSParseError(pdu, "The SMS's first octet indicates it has UDH, but its UDH length is 0");
		if (ud.size() < ud[0] + 1)
		{
			std::ostringstream errorReason;
			errorReason << "The SMS's first octet indicates it has UDH, but UDH's length ("
						<< static_cast<unsigned int>(ud[0]) << ") + 1 exceeds UD size (" << ud.size() << ")";
			throw Utils::Error::SMSParseError(pdu, errorReason.str());
		}
		switch (ud[1])
		{
		case 0x00:
		case 0x08:
			// 8-bit reference or 16-bit reference for SMS segment
			if (ud[0] < 2)
			{
				//throw: no IEDL
				std::ostringstream errorReason;
				errorReason << "The SMS's UDH indicates it is an SMS segment, but there is no IEDL in its UDH because its UDHL = "
							<< static_cast<unsigned int>(ud[0]);
				throw Utils::Error::SMSParseError(pdu, errorReason.str());
			}
			else if (ud[0] != 2 + ud[2])
			{
				//else: UD[0] --- UDH length != IEI + IEDL + length of IED
				std::ostringstream errorReason;
				errorReason << "The SMS's UDH indicates it is an SMS segment, but the IEDL in its UDH does not match its UDHL = "
							<< static_cast<unsigned int>(ud[0]) << ", whereas IEDL = "
							<< static_cast<unsigned int>(ud[2]) << ", but UDH must be 2 + IDEL for this case";
				throw Utils::Error::SMSParseError(pdu, errorReason.str());
			}
			else if (ud[2] == 3)
			{
				// IEDL = 3: 1 reference, 1 count, 1 index
				reference = ud[3];
				count = static_cast<unsigned int>(ud[4]);
				index = static_cast<unsigned int>(ud[5]) - 1;

			}
			else if (ud[2] == 4)
			{
				// IEDL = ud[2]
				reference = ud[3] << 8 | ud[4];
				count = static_cast<unsigned int>(ud[5]);
				index = static_cast<unsigned int>(ud[6]) - 1;
			}
			else
			{
				//throw: unexpected IEDL 
				std::ostringstream errorReason;
				errorReason << "The SMS's UDH indicates it is an SMS segment, but the IEDL in its UDH is unexpected: "
							<< static_cast<unsigned int>(ud[2]);
				throw Utils::Error::SMSParseError(pdu, errorReason.str());
			}
			is_segment = true;
			break;
		default:
		{
			std::ostringstream errorReason;
			errorReason << "The SMS's first octect indicates it has UDH, but the IEI in UDH is "
				        << std::hex << ud[1] << ": not recognizable";
			throw Utils::Error::SMSParseError(pdu, errorReason.str());
		}
		}
	}


    switch (dcs & 0x0C)
    {
    case 0x08:
        content = decodeUCS2(ud, skip);
        break;
    case 0x00:
        content = decodeGSM7(ud, skip, static_cast<unsigned int>(udl));
        break;
    default:
    {
        std::ostringstream errorReason;
        errorReason << " cannot recognize DCS field " << std::hex << dcs;
        throw Utils::Error::SMSParseError(pdu, errorReason.str());
    }
    }

	if (is_segment && reference != 0)
	{
		if (auto segments = ref_to_segments.find(reference); segments != ref_to_segments.end())
		{
			auto& all_contents = std::get<1>(segments->second);
			if (all_contents.size() <= index)
			{
				std::ostringstream errorReason;
				errorReason << "Got a SMS segment; the reference exists, but it expects "
							<< all_contents.size() << " segments, whereas the current segment index is "
							<< index;
				throw Utils::Error::SMSParseError(pdu, errorReason.str());
			}
			if (all_contents.size() != count)
			{
				std::ostringstream errorReason;
				errorReason << "Got a SMS segment; the reference exists, but it expects "
							<< all_contents.size() << " segments, whereas this segment says it is the No. "
							<< index << "segment of " << count << " in total";
				throw Utils::Error::SMSParseError(pdu, errorReason.str());
			}
			if (all_contents[index].empty()) ++std::get<0>(segments->second);
			all_contents[index] = content;
		}
		else
		{
			std::vector<std::string> all_contents;
			all_contents.resize(count);
			all_contents[index] = content;
			ref_to_segments.emplace(reference, std::make_tuple((unsigned short)1, all_contents));
		}
	}

}

