#ifndef COMMAND_PIPE_HPP
#define COMMAND_PIPE_HPP

#include <string>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "serial_interface.hpp"

namespace Utils
{

    enum class Role
    {
        SERVICE = 0,
        CLIENT = 1,
    };

    class CommandPipe
    {
    public:
        static constexpr const char *PIPE_PATH[]{
            "/tmp/command_pipe_s2c",
            "/tmp/command_pipe_c2s"};

        explicit CommandPipe(Role role);

        CommandPipe(const CommandPipe &) = delete;
        CommandPipe &operator=(const CommandPipe &) = delete;

        void listen(std::function<void(std::shared_ptr<Interface::AMessage>)> callback);

        // Send a string into the pipe.
        void send(const std::shared_ptr<Interface::AMessage>& message);

		void close();

    private:
        Role role_ = Role::SERVICE;
        unsigned int write_idx = 0;
        unsigned int listen_idx = 1;

		std::ifstream ifs;
    };
}

#endif
