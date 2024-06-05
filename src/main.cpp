#include "domain/entity/session.hpp"
#include "server/server.hpp"

#include <cstdlib>
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
    SET_BINARY_MODE();

    server::Server server(std::make_shared<domain::entity::IOSession>());

    server.Run();

    return EXIT_SUCCESS;
}
