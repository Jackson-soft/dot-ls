#include "server/server.hpp"

#include <cstdlib>
#include <iostream>

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
    hesiod::server::LanguageServer server;
    if (argc > 1) {
        int port = std::stoi(argv[1]);
    } else {
        SET_BINARY_MODE();

        while (true) {
            std::cin.peek();
            char *buffer{nullptr};
            std::cin.readsome(buffer, 128);

            std::cout.write("sfsfs\n", 7);

            auto srv = hesiod::server::LanguageServer();

            std::cout.flush();
        }
    }

    server.Run();

    return EXIT_SUCCESS;
}
