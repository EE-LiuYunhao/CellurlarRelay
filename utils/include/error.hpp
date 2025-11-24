#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <exception>
#include <string>
#include <iostream>
#include <sstream>
#include <optional>
#include <curl/curl.h>

namespace Utils::Error
{
    enum class Type
    {
        UNEXPECTED_AT_RESPONDSE = 1,
        PARSER_ERROR = 2,
        PIPE_ERROR = 3,
        SMS_PDU_ERROR = 4,
        EMAIL_ERROR = 5
    };

    std::ostream &operator<<(std::ostream &os, const Type &type);

    template <Type ET>
    class BaseError : public std::exception
    {
    public:
        explicit BaseError(std::string msg) : message_(std::move(msg))
        {
            std::ostringstream oss;
            oss << getType() << ": " << message_;
            error_what = oss.str();
        }

        const char *what() const noexcept override
        {
            return error_what.c_str();
        }

        constexpr Type getType() const
        {
            return ET;
        }

        BaseError(BaseError &&other) = default;

        BaseError(const BaseError &other) = default;

        BaseError &operator=(BaseError &&other) = delete;

        BaseError &operator=(const BaseError &other) = delete;

    private:
        const std::string message_;

        std::string error_what;
    };
    /**
     * Register the handler when the app is crashed. Only call this method in the MAIN.
     */
    void crash_printer(int sig);

    class UnexpectedATResponse : public BaseError<Type::UNEXPECTED_AT_RESPONDSE>
    {
    public:
        UnexpectedATResponse(std::string at_command, std::string expected, std::string got);
    };

    class SMSParseError : public BaseError<Type::SMS_PDU_ERROR>
    {
    public:
        SMSParseError(const std::string &raw_pdu, const std::string &parse_detail);
    };

    class ParserError : public BaseError<Type::PARSER_ERROR>
    {
    public:
        ParserError(std::string message);
    };

    class PipeError : public BaseError<Type::PIPE_ERROR>
    {
    public:
        PipeError(std::string message);
    };

    class EmailError : public BaseError<Type::EMAIL_ERROR>
    {
    public:
        EmailError(std::optional<CURLcode> curlErrorCode, std::string description);
    };
}

#endif // LOGGER_HPP
