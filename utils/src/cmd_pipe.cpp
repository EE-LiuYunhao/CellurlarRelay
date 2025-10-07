#include "cmd_pipe.hpp"
#include "error.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Utils;

CommandPipe::CommandPipe(Role role): role_(role)
{
    // Create FIFO if it doesn't exist
    struct stat st;
    for (unsigned int idx = 0; idx < 2; idx++)
    {
        std::cout << "FIFO pipe " << PIPE_PATH[idx] << " is not created yet. mkfifo it first" << std::endl;
        if (stat(PIPE_PATH[idx], &st) != 0)
        {
            if (mkfifo(PIPE_PATH[idx], 0666) == -1)
            {
                throw std::runtime_error("Failed to create named pipe");
            }
        }
    }
    switch (role)
    {
    case Role::SERVICE:
        std::cout << "Service-end ";
        break;
    case Role::CLIENT:
        std::cout << "Client-end ";
        break;
    }
    write_idx = static_cast<unsigned int>(role);
    listen_idx = 1 - write_idx;

    if (write_idx > 1 || listen_idx > 1)
    {
        throw Error::PipeError("invalid index to select the FIFO pipe path");
    }

    std::cout << "of the command pipe is launched, listenging to " << PIPE_PATH[listen_idx]
              << " and writing to " << PIPE_PATH[write_idx] << std::endl;
}

void CommandPipe::listen(std::atomic<bool> &stop, std::function<void(std::shared_ptr<Interface::AMessage>)> callback)
{
    std::ifstream ifs;
    ifs.basic_ios<char>::rdbuf()->pubsetbuf(nullptr, 0); // unbuffered
    ifs.open(CommandPipe::PIPE_PATH[listen_idx]);

    if (!ifs.is_open())
    {
        throw Error::PipeError(std::string("fail to open ") + PIPE_PATH[listen_idx] + " as an std::ifstream");
    }

    std::string line;
    while (!stop)
    {
        callback(Interface::parse(ifs));
    }
    ifs.close();
}

void CommandPipe::send(const std::shared_ptr<Interface::AMessage>& message)
{
    switch (role_)
    {
    case Role::SERVICE:
        std::cout << "Service-end ";
        break;
    case Role::CLIENT:
        std::cout << "Client-end ";
        break;
    }
    std::cout << "writes: " << message << " to pipe" << PIPE_PATH[write_idx] << std::endl;

    std::ofstream ofs(CommandPipe::PIPE_PATH[write_idx]);
    if (!ofs.is_open())
    {
        throw Error::PipeError(std::string("fail to open ") + PIPE_PATH[listen_idx] + " as an std::ofstream");
    }
    ofs << message;
    ofs.close();
}
