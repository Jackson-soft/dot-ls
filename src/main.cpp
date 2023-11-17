#include <cstdlib>
#include <format>
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
    std::cout << std::format("sff {}", 233) << std::endl;
    if (argc > 1) {
    } else {
        SET_BINARY_MODE();

        while (true) {
            std::cin.peek();
            char *buffer{nullptr};
            std::cin.readsome(buffer, 128);

            std::cout.write("sfsfs\n", 7);

            std::cout.flush();
        }
    }

    return EXIT_SUCCESS;
}
