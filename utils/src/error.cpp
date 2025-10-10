#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <execinfo.h>

#include "error.hpp"

namespace Utils::Error
{
    std::ostream &operator<<(std::ostream &os, const Type &type)
    {
        os << "ERROR [" << static_cast<unsigned int>(type) << " (";
        switch (type)
        {
        case Type::UNEXPECTED_AT_RESPONDSE:
            os << "UNEXPECTED_AT_RESPONDSE";
            break;
        case Type::PARSER_ERROR:
            os << "PARSER_ERROR";
            break;
        case Type::PIPE_ERROR:
            os << "PIPE_ERROR";
            break;
        }
        os << ")]";
    }

    ParserError::ParserError(std::string message) : BaseError(std::move(message)) {}

    PipeError::PipeError(std::string message) : BaseError(std::move(message)) {}

    UnexpectedATResponse::UnexpectedATResponse(std::string at_command, std::string expected, std::string got) : BaseError(at_command + " >>> expecting " + expected + "; got " + got) {}

    void crash_printer(int sig)
    {
        void *buffer[64];
        int nptrs = backtrace(buffer, 64);

        // Print signal info
        dprintf(STDERR_FILENO, "\n*** Caught signal %d (%s) ***\n", sig, strsignal(sig));

        // Print backtrace
        backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);

        // Restore default and re-raise to terminate with proper status
        signal(sig, SIG_DFL);
        raise(sig);
    }
}