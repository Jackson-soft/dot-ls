#include "basis/boot/boot.hpp"
#include "domain/entity/session.hpp"
#include "server/server.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/cobalt/run.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
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

auto main(int argc, char **argv) -> int {
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce a help message")("version,v", "print version string");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.count("help") != 0U) {
        std::cout << desc << '\n';
        return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0U) {
        std::print("{}\n", basic::boot::Version);
        return EXIT_SUCCESS;
    }

    SET_BINARY_MODE();

    if (!basic::boot::Boot()) {
        return EXIT_FAILURE;
    }

    uranus::utils::LogHelper::Instance().Info("server start");

    server::Server server(std::make_shared<domain::entity::IOSession>());

    boost::asio::thread_pool tp{3};

    boost::cobalt::spawn(tp.get_executor(), server.Run(), boost::asio::detached);

    tp.join();

    return EXIT_SUCCESS;
}