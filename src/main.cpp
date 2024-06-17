#include "domain/entity/session.hpp"
#include "server/server.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>

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
    desc.add_options()("help,h", "produce a help message")("stdio",
                                                           boost::program_options::value<std::string>(),
                                                           "just an option");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return EXIT_SUCCESS;
    }

    SET_BINARY_MODE();

    server::Server server(std::make_shared<domain::entity::IOSession>());

    server.Run();

    return EXIT_SUCCESS;
}
