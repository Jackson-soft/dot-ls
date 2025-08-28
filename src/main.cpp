#include "basis/boot/boot.hpp"
#include "server/server.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/cobalt.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdlib>
#include <iostream>
#include <print>

// https://stackoverflow.com/questions/1598985/c-read-binary-stdin
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define SET_BINARY_MODE()                \
    _setmode(_fileno(stdin), _O_BINARY); \
    _setmode(_fileno(stdout), _O_BINARY)
#else
#define SET_BINARY_MODE() (void)0
#endif

boost::cobalt::main co_main(int argc, char **argv) {
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce a help message")("version,v", "print version string");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.contains("help")) {
        std::cout << desc << '\n';
        co_return EXIT_SUCCESS;
    }
    if (vm.contains("version")) {
        std::print("{}\n", basic::boot::Version);
        co_return EXIT_SUCCESS;
    }

    SET_BINARY_MODE();
    /*
            if (!basic::boot::Boot()) {
                return EXIT_FAILURE;
            }

            uranus::utils::LogHelper::Instance().Info("server start");
        */

    boost::asio::io_context io;

    server::Server server(io);

    co_await server.Run();

    co_return EXIT_SUCCESS;
}