all:

release:
	conan install conanfile.txt -b=missing -u

debug:
	conan install conanfile.txt -b=missing -u -s build_type=Debug

install:
	conan install conanfile.txt -b=missing -u

build:
	cmake --build --preset conan-debug