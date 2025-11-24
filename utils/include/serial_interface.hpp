
#include <iostream>
#include <string>
#include <memory>
#include <optional>

namespace Utils::Interface
{
    enum class Type
    {
        UNKNOWN = 0,
        COMMAND = 1,
        PROMPT = 2
    };

    class AMessage
    {
    public:

        explicit AMessage(Type direction);
    
        friend std::ostream& operator<<(std::ostream& os, const std::shared_ptr<AMessage>& cmd);

        virtual std::string message() const = 0;

    protected:
        virtual std::string to_string() const = 0;


    private:
        Type type_;
    };

    class Command : public AMessage
    {
    public:

        Command(std::string at_command, std::optional<std::string> expected_respond);

        Command(const Command&) = default;

        Command(Command&&) = default;

        Command& operator=(const Command&) = delete;

        Command& operator=(Command&&) = delete;

        void verify(const std::string& respond) const;

        std::string message() const override;

    protected:
        std::string to_string() const override;
    
    private:
        const std::string at_command_;
        const std::optional<std::string> expected_respond_;
    };

    class Prompt : public AMessage
    {
    public:

        explicit Prompt(std::string message);

        Prompt(const Prompt&) = default;

        Prompt(Prompt&&) = default;

        Prompt& operator=(const Prompt&) = delete;

        Prompt& operator=(Prompt&&) = delete;

        std::string message() const override;

    protected:
        std::string to_string() const override;
    
    private:
        const std::string message_;
    };

    std::shared_ptr<AMessage> parse(std::istream& is);
}
