#include "serial_interface.hpp"
#include <sstream>
#include "error.hpp"

namespace Utils::Interface
{
    AMessage::AMessage(Type direction) : type_(std::move(direction)) {}

    std::ostream &operator<<(std::ostream &os, const std::shared_ptr<AMessage> &cmd)
    {
        if (cmd == nullptr)
        {
            os << 0;
            return os;
        }

        os << static_cast<unsigned int>(cmd->type_);
        os << cmd->to_string();
        return os;
    }

    Command::Command(std::string at_command, std::optional<std::string> expected_respond) : AMessage(Type::COMMAND), at_command_(std::move(at_command)),
                                                                                            expected_respond_(std::move(expected_respond)) {}

    std::string Command::to_string() const
    {
        std::ostringstream oss;
        oss << at_command_;
        if (expected_respond_)
        {
            oss << ';' << expected_respond_.value();
        }
        return oss.str();
    }

    std::string Command::message() const 
    {
        return at_command_;
    }

    void Command::verify(const std::string &respond) const
    {
        if (!expected_respond_.has_value())
        {
            std::cout << "AT COMMAND (" << at_command_ << ") has no respond expectation; got "
                      << respond << std::endl;
        }
        else if (respond != *expected_respond_)
        {
            std::cout << "AT COMMAND (" << at_command_ << ") verification fails; expecting "
                      << *expected_respond_ << ", got " << respond << " -> FATAL ERROR!" << std::endl;
            throw Error::UnexpectedATResponse(at_command_, *expected_respond_, respond);
        }
        else
        {
            std::cout << "AT COMMAND (" << at_command_ << ") verification succeeds; got "
                      << respond << " as expected" << std::endl;
        }
    }

    Prompt::Prompt(std::string message) : AMessage(Type::PROMPT),
                                          message_(std::move(message)) {}

                                          
    std::string Prompt::message() const 
    {
        return message_;
    }

    std::string Prompt::to_string() const
    {
        return message_;
    }

    std::shared_ptr<AMessage> parse(std::istream &is)
    {
        unsigned int type;
        is >> type;
        if (is.fail() || type == 0)
        {
            throw Error::ParserError("fail to parse the message between client and service; got service type 0 (UNKNOWN)");
        }

        switch (static_cast<Type>(type))
        {
        case Type::COMMAND:
        {
            std::string content0;
            char ch;
            while (is.get(ch))
            {
                if (ch == ';' || ch == '\n')
                    break;
                content0.push_back(ch);
            }
            if (ch == ';')
            {
                std::string content1;
                while (is.get(ch))
                {
                    if (ch == '\n')
                        break;
                    content1.push_back(ch);
                }
                std::cout << "Parse input stream to AT COMMAND: {" << content0 << ';' << content1
                          << '}' << std::endl;
                return std::make_shared<Command>(content0, content1);
            }
            std::cout << "Parse input stream to AT COMMAND: {" << content0 << "} WITH NO expected respond"
                      << std::endl;
            return std::make_shared<Command>(content0, std::nullopt);
        }
        case Type::PROMPT:
        {
            std::string content;
            std::getline(is, content);
            return std::make_shared<Prompt>(content);
        }
        default:
            throw Error::ParserError("fail to parse the message between client and service; got service type 0 (UNKNOWN)");
        }
    }
}
