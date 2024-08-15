all:

release:
	conan install conanfile.txt -b=missing -u

debug:
	conan install conanfile.txt -b=missing -u -s build_type=Debug

config:
	cmake --preset conan-debug

target:
	cmake --build build --preset conan-debug
