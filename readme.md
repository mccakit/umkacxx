conan remove "*:*" -c
conan install . --build=missing --profile=native_debug_asan -of ./conan --deployer=full_deploy --envs-generation=false
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$HOME/dev/toolchains/static/debug/cmake/native/asan.cmake
