#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <yaml-cpp/yaml.h>
#include <memory>
#include <mutex>
#include <string>

namespace Utils::Options
{

class Base
{
public:
    Base();
    
    Base(const Base& other) = default;
    Base(Base&& other) = default;
    Base& operator=(const Base& other) = default;
    Base& operator=(Base&& other) = default;

    ~Base() = default;

protected:

    static std::shared_ptr<YAML::Node> all_configs;
    static std::once_flag config_init_flag;

};

class Email: public Base
{
public:

    Email();
    Email(const Email& other) = default;
    Email(Email&& other) = default;
    Email& operator=(const Email& other) = default;
    Email& operator=(Email&& other) = default;

    bool is_valid() const;

    std::string get_sender_email() const;
    std::string get_sender_bracket() const;
    std::string get_sender() const;
    std::string get_server() const;
    std::string get_password() const;
    std::string get_receiver() const;
    std::string get_receiver_bracket() const;

private:

    bool m_valid;


    std::string sender_email;
    std::string sender_bracket;
    std::string sender;
    std::string server;
    std::string password;

    std::string receiver;
    std::string receiver_bracket;


};

} // namespace Utils::Options


#endif // OPTIONS_HPP
