#include "serial.hpp"
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

class Service
{
public:
    Service(unsigned int powerkey)
    {
        m_serial.begin(115200);
        auto hi = send_command_get_respond("AT", 2000ms);
        if (!hi)
        {
            std::cout << "starting serial at /dev/ttyS0" << std::endl;
        }
        SerialPi::pinMode(powerkey, OUTPUT);
        SerialPi::digitalWrite(powerkey, HIGH);
        std::this_thread::sleep_for(600ms);
        SerialPi::digitalWrite(powerkey, LOW);
        for (std::optional<std::string> hi = std::nullopt; !hi; hi = send_command_get_respond("AT", 2000ms))
        {
        }

        std::this_thread::sleep_for(500ms);

        send_command_get_respond("AT+CREG?", 500ms);
    }

    ~Service()
    {
        m_stop = true;
        if (m_frontend_thread != nullptr && m_frontend_thread->joinable())
            m_frontend_thread->join();
        m_serial.end();
    }

    Service(const Service &) = delete;

    Service(Service &&) = default;

    Service &operator=(const Service &) = delete;

    Service &operator=(Service &&) = default;

    void loop()
    {
        std::ostringstream incoming;
        char last = 0;
        send_command_get_respond("AT+CMGF=1", 1000ms);
        send_command_get_respond("AT+CSCS=\"UCS2\"", 1000ms);
        send_command_get_respond("AT+CNMI=2,1", 1000ms);
        while (!m_stop)
        {
            auto first = m_serial.receive();
            if (m_serial.last_read_cnt <= 0)
            {
                std::cout << "Serial port is closed;" << std::endl;
                break;
            }
            if (first == '\n' && last == '\r')
            {
                auto content = incoming.str();
                incoming.clear();
                if (const auto cmti = content.find("+CMTI:"); cmti != content.npos)
                {
                    if (const auto sm_cnt = content.find("\"SM\","); sm_cnt != content.npos)
                    {
                        std::ostringstream query_formatter;
                        query_formatter << "AT+CMGR=" << content.substr(sm_cnt);
                        auto content = send_command_get_respond(query_formatter.str().c_str(), 2000ms);
                        if (content)
                            sms_handler(*content);
                    }
                }
                else if (const auto ata = content.find("ATA"); ata != content.npos)
                {
                    m_pipe.send(std::make_shared<Utils::Interface::Prompt>("Incoming phone call"));
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
                        catch (std::exception error)
                        {
                            std::cout << error.what() << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "Unparsable content from serial port: " << content;
                    }
                }
            }
            else if (first != '\r')
            {
                incoming << first;
            }
            last = first;
        }
        std::cout << "Daemon loop ends" << std::endl;
    }

    void begin_daemon_thread()
    {
        if (m_frontend_thread == nullptr)
        {
            m_frontend_thread = std::make_unique<std::thread>([this]()
                                                              { m_pipe.listen(m_stop, [this](auto msg)
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

        if (m_serial.available())
        {
            std::ostringstream oss;
            bool oFound = false;
            while (m_serial.available() || std::chrono::steady_clock::now() - start <= timeout)
            {
                auto c = m_serial.receive();
                if (!oFound && c == 'O')
                {
                    oFound = true;
                }
                else if (oFound)
                {
                    if (c == 'K')
                        break;
                    else
                        oFound = false;
                    oss << 'O' << c;
                }
                else if (c != '\r')
                {
                    oss << c;
                }
            }
            std::cout << "Sent " << command << ", got " << oss.str() << std::endl;
            return oss.str();
        }
        std::cout << "Sent " << command << ", timeout" << std::endl;
        return std::nullopt;
    }

    void frontend_request_handler(std::shared_ptr<Utils::Interface::AMessage> incoming_request)
    {
        const auto command = std::dynamic_pointer_cast<Utils::Interface::Command>(incoming_request);
        if (command == nullptr)
        {
            std::cout << "daemon thread receive non-command message, ignore" << std::endl;
            return;
        }
        m_serial.println(command->message().c_str());
        std::unique_lock lock(m_cmd_queue_mtx);
        m_incoming_commands.emplace(std::move(command));
    }

    void sms_handler(std::string raw_msg)
    {
        //todo: parse the message, send to email
    }

private:
    SerialPi m_serial;

    std::unique_ptr<std::thread> m_frontend_thread;

    std::atomic<bool> m_stop;

    std::queue<std::shared_ptr<Utils::Interface::Command>> m_incoming_commands;

    std::mutex m_cmd_queue_mtx;

    Utils::CommandPipe m_pipe{Utils::Role::SERVICE};
};

static std::unique_ptr<Service> ptr = nullptr;

void sig_int_handler(int sig)
{
    ptr = nullptr;
}

int main()
{
    std::signal(SIGINT, sig_int_handler);
    std::signal(SIGABRT, Utils::Error::crash_printer);
    std::signal(SIGSEGV, Utils::Error::crash_printer);
    std::signal(SIGFPE, Utils::Error::crash_printer);
    std::signal(SIGILL, Utils::Error::crash_printer);
    std::signal(SIGBUS, Utils::Error::crash_printer);

    ptr = std::make_unique<Service>();
    ptr->begin_daemon_thread();
    ptr->loop();
    return 0;
}