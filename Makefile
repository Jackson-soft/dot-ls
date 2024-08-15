all:

release:
	conan install conanfile.txt -b=missing -u

debug:
	conan install conanfile.txt -b=missing -u -s build_type=Debug

install:
	cmake --preset conan-debug

build:
	cmake --build build --preset conan-debug
