#include <sstream>
#include <iostream>
#include "options.hpp"

namespace Utils::Options
{

namespace
{

constexpr std::string_view CONFIG_PATH = "/etc/cellular_uart_service/config.yaml";

} // Anonymous namespace


std::shared_ptr<YAML::Node> Base::all_configs = nullptr;
std::once_flag Base::config_init_flag;

Base::Base()
{
    std::call_once(config_init_flag, [this]()
    {
        try 
        {
            all_configs = std::make_shared<YAML::Node>(YAML::LoadFile(CONFIG_PATH.data()));
        }
        catch (const YAML::BadFile& e)
        {
            std::cerr << "Failed to load config yaml at " << CONFIG_PATH << " because " << e.what();
        }
    });
}

Email::Email(): Base(), m_valid(true)
{
    if (all_configs == nullptr)
    {
        m_valid = false;
        return;
    }
    // Sender block
    if ((*all_configs)["sender"] && (*all_configs)["sender"].IsMap())
    {
        auto sender_config = (*all_configs)["sender"];

        try 
        {
            sender_email = sender_config["email"].as<std::string>();
            server = sender_config["server"].as<std::string>();
            password = sender_config["password"].as<std::string>();

            std::ostringstream bracket_formatter;
            bracket_formatter << '<' << sender_email << '>';
            sender_bracket = bracket_formatter.str();

            sender = sender_config["name"].as<std::string>("") + sender_bracket;
            std::cout << "Config: sender as " << sender << " to " << server << std::endl;

        }
        catch (const YAML::Exception& e)
        {
            std::cerr << "The config yaml at " << CONFIG_PATH << " does not have 'email', 'server' and 'password' in its "
                         "sender block, which are all required. The received SMS will nt be sent but store locally to the log"
                         ". The error is: " << e.what() << std::endl;
            m_valid = false;
        }

    }
    else
    {
        std::cerr << "The config yaml at " << CONFIG_PATH << " does not have a 'sender' block; the SMS will not be sent but"
                     " store locally to the log" << std::endl; 
        m_valid = false;
    }

    // Receiver block
    if ((*all_configs)["receiver"] && (*all_configs)["receiver"].IsMap())
    {
        auto receiver_config = (*all_configs)["receiver"];

        try 
        {
            std::string email = receiver_config["email"].as<std::string>();

            std::ostringstream bracket_formatter;
            bracket_formatter << '<' << email << '>';
            receiver_bracket = bracket_formatter.str();

            receiver = receiver_config["name"].as<std::string>("") + receiver_bracket;
            std::cout << "Config: receiver is set to " << receiver << std::endl;
        }
        catch (const YAML::Exception& e)
        {
            std::cerr << "The config yaml at " << CONFIG_PATH << " does not have 'email' field in its "
                         "receiver block, which is required. The received SMS will nt be sent but store locally to the log"
                         ". The error is: " << e.what() << std::endl;
            m_valid = false;
        }
    }
    else
    {
        std::cerr << "The config yaml at " << CONFIG_PATH << " does not have a 'receiver' block; the SMS will not be sent but"
                     " store locally to the log" << std::endl; 
        m_valid = false;
    }
}


std::string Email::get_sender_email() const { return sender_email; }
std::string Email::get_sender_bracket() const { return sender_bracket; }
std::string Email::get_sender() const { return sender; }
std::string Email::get_server() const { return server; }
std::string Email::get_password() const { return password; }
std::string Email::get_receiver() const { return receiver; }
std::string Email::get_receiver_bracket() const { return receiver_bracket; }
bool Email::is_valid() const { return m_valid; }


}// namespace Utils::Options
