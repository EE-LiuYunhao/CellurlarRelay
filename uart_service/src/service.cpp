#include "serial.hpp"
#include "sms.hpp"
#include "cmd_pipe.hpp"
#include "error.hpp"

#include <memory>
#include <string>
#include <chrono>
#include <sstream>
#include <iostream>
#include <optional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <csignal>

using namespace std::literals::chrono_literals;

constexpr int POWERKEY = 6;

class Service
{
public:
    Service(unsigned int powerkey)
    {
        m_serial.begin(115200);
        std::cout << "starting serial at /dev/ttyS0" << std::endl;
        // SerialPi::pinMode(powerkey, OUTPUT);
		// std::cout << "\tPin mode set" << std::endl;
        // SerialPi::digitalWrite(powerkey, HIGH);
		// std::cout << "\tDigital write set to HIGH" << std::endl;
        // std::this_thread::sleep_for(600ms);
        // SerialPi::digitalWrite(powerkey, LOW);
		// std::cout << "\tDigital write set to LOW" << std::endl;
        for (std::optional<std::string> hi = std::nullopt; !hi; hi = send_command_get_respond("AT", 2000ms))
        {
        }

        std::this_thread::sleep_for(500ms);

		for (std::optional<std::string> ready_or_not = std::nullopt; 
			 ready_or_not == std::nullopt || ready_or_not->find("CPIN: READY") == std::string::npos;
			 ready_or_not = send_command_get_respond("AT+CPIN?", 500ms))
		{
			std::this_thread::sleep_for(500ms);
		}
		// query signal connection
        send_command_get_respond("AT+CREG?", 500ms);
        // query carrier
        send_command_get_respond("AT+COPS?", 1500ms);
		// force disable internet
        send_command_get_respond("AT+CGATT=0", 1500ms);
		// enable the phone call number
        send_command_get_respond("AT+CLIP=1", 1500ms);
        // query signal
        send_command_get_respond("AT+CSQ", 1500ms);
    }

    ~Service()
    {
        m_serial.end();
		m_pipe.close();
    }

    Service(const Service &) = delete;

    Service(Service &&) = default;

    Service &operator=(const Service &) = delete;

    Service &operator=(Service &&) = default;

    void loop()
    {
        std::ostringstream incoming;
        char last = 0;
        send_command_get_respond("AT+CMGF=0", 1000ms); // PDU mode
        send_command_get_respond("AT+CNMI=2,1", 1000ms);
        while (true)
        {
            auto first = m_serial.receive();
			if (first == 0 || first == 4)
			{
				std::cout << "serial port is closed or gets EOF (the other end is off-line)" << std::endl;
				break;
			}
            if (first == '\n' && last == '\r')
            {
                auto content = incoming.str();
				incoming.str("");
                incoming.clear();
				if (content.empty())
				{
					last = 0;
					continue; // empty -> skip
				}
                if (const auto cmti = content.find("+CMTI:"); cmti != content.npos)
                {
					std::cout << "New SMS: " << content; 
                    if (const auto sm_cnt = content.find("\"SM\","); sm_cnt != content.npos)
                    {
                        std::ostringstream query_formatter;
                        std::ostringstream delete_formatter;
						const auto smsNo = content.substr(sm_cnt + 5);
                        query_formatter << "AT+CMGR=" << smsNo;
                        delete_formatter << "AT+CMGD=" << smsNo;
						std::cout << " -> No. " << smsNo << std::endl;
                        auto smsContent = send_command_get_respond(query_formatter.str(), 2000ms);
                        send_command_get_respond(delete_formatter.str(), 1000ms);
						if (smsContent)
						{
                            sms_handler(*smsContent);
						} 
						else
						{
							std::cerr << query_formatter.str() << " => no message returned" << std::endl; 
						}
                    }
					else 
					{
						std::cout << " -> Unparsable number" << std::endl;
					}
                }
				else if (const auto ring = content.find("RING"); ring != content.npos)
                {
					if (const auto phone = content.find("CLIP:"); phone != content.npos)
					{
						const auto phone_num = content.substr(phone, content.find(",", phone));
						std::cout << "New incoming phone call from " << phone_num << std::endl;
					}
					else
					{
						std::cout << "New incoming phone call from unknown caller (NO CLIP entry"
							      << ", raw content: {" << content << "})" << std::endl;
					}
                }
                else
                {
                    std::unique_lock lock(m_cmd_queue_mtx);
                    if (!m_incoming_commands.empty())
                    {
                        try
                        {
                            auto latestCommand = m_incoming_commands.front();
                            m_incoming_commands.pop();
                            latestCommand->verify(content);
                            std::cout << "Command from front-end: " << latestCommand->message() << " gets expected result " << content << std::endl;
                            m_pipe.send(std::make_shared<Utils::Interface::Prompt>(content));
                        }
                        catch (const std::exception &error)
                        {
                            std::cerr << error.what() << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "Unparsable content from serial port: " << content;
                    }
                }
            }
            else if (first != '\r')
            {
                incoming << first;
            }
            last = first;
        }
        std::cout << "loop ends" << std::endl;
    }

    void begin_daemon_thread()
    {
        if (m_frontend_thread == nullptr)
        {
            m_frontend_thread = std::make_unique<std::thread>([this]()
                                                              { m_pipe.listen([this](auto msg)
                                                                              { frontend_request_handler(msg); }); });
            std::cout << "Daemon is listening to front-end" << std::endl;
        }
    }

protected:
    std::optional<std::string> send_command_get_respond(const std::string &command, std::chrono::milliseconds timeout)
    {
        m_serial.flush();
        m_serial.println(command.c_str());
        auto start = std::chrono::steady_clock::now();

        // spin for respond
        while (!m_serial.available() && std::chrono::steady_clock::now() - start <= timeout)
        {
        } // wait

        std::ostringstream oss;
        bool oFound = false;
		char c;
        while (m_serial.available() || std::chrono::steady_clock::now() - start <= timeout)
        {
			int remainedTime = (timeout - (std::chrono::steady_clock::now() - start)).count();

            c = m_serial.receive(remainedTime >= 0 ? remainedTime : 0);
			if (c == 26 || c == 0 || c == 4)
			{
				// 26 --- ^Z timeout
				// 0 --- error
				// 4 --- EOF
				break;
			}
            if (!oFound && c == 'O')
            {
                oFound = true;
            }
            else if (oFound)
            {
                if (c == 'K')
				{
					std::cout << "Sent " << command << ", got " << oss.str() << std::endl;
					return oss.str();
				}
                oFound = false;
                oss << 'O' << c;
            }
            else if (c != '\r')
            {
                oss << c;
            }
        }
		if (c == 0) std::cerr << "Sent " << command << ", error" << std::endl;
		else if (c == 4) std::cout << "Sent " << command << ", but the serial port is closed" << std::endl;
		else std::cerr << "Sent " << command << ", timeout" << std::endl;
		return std::nullopt;
    }

    void frontend_request_handler(std::shared_ptr<Utils::Interface::AMessage> incoming_request)
    {
        const auto command = std::dynamic_pointer_cast<Utils::Interface::Command>(incoming_request);
        if (command == nullptr)
        {
            std::cerr << "daemon thread receive non-command message, ignore" << std::endl;
            return;
        }
        m_serial.println(command->message().c_str());
        std::unique_lock lock(m_cmd_queue_mtx);
        m_incoming_commands.emplace(std::move(command));
    }

    static inline void trim(std::string &s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch)
                                        { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(),
                             [](unsigned char ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
    }

    void sms_handler(std::string raw_msg)
    {
        size_t pos = raw_msg.find("\n");
		pos = raw_msg.find("\n", pos + 1);
        if (pos == std::string::npos)
        {
            std::cerr << "cannot handle the received SMS raw string: " << raw_msg
					  << "; only one line is presented. No PDU line" << std::endl;
        }

        // Everything after that line is the PDU
        auto pdu = raw_msg.substr(pos + 1);
        trim(pdu);
        try
        {
            SMS message(pdu);
			std::cout << "Parsed to " << message << std::endl;
            message.send_email();
        }
        catch (const std::exception &exp)
        {
            std::cerr << exp.what() << std::endl;
        }
    }

private:
    SerialPi m_serial;

    std::unique_ptr<std::thread> m_frontend_thread;

    std::queue<std::shared_ptr<Utils::Interface::Command>> m_incoming_commands;

    std::mutex m_cmd_queue_mtx;

    Utils::CommandPipe m_pipe{Utils::Role::SERVICE};
};

static std::unique_ptr<Service> ptr = nullptr;

void sig_int_handler(int sig)
{
	std::cout << "Killed by ";
	if (sig == SIGINT) std::cout << " Ctrl-C ";
	if (sig == SIGTERM) std::cout << " KILL ";
	std::cout << std::endl;
	
    ptr = nullptr;
}

int main()
{
    std::signal(SIGINT, sig_int_handler);
    std::signal(SIGTERM, sig_int_handler);
    std::signal(SIGABRT, Utils::Error::crash_printer);
    std::signal(SIGSEGV, Utils::Error::crash_printer);
    std::signal(SIGFPE, Utils::Error::crash_printer);
    std::signal(SIGILL, Utils::Error::crash_printer);
    std::signal(SIGBUS, Utils::Error::crash_printer);

    ptr = std::make_unique<Service>(POWERKEY);
    ptr->begin_daemon_thread();
    ptr->loop();
    return 0;
}
