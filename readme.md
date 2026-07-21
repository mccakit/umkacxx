# umkacxx

Umkacxx is a C++ library that provides a C++ module interface for the [umka](https://github.com/umka-lang/umka), a statically typed, interpreted scripting language.

Project is built using CMake and packaged via CPS. CMake 4.4 and later is required, ninja is recommended for building.

Building:

```bash
# You can find options via inspecting options.cmake
cmake -S ${srcdir} -B ${builddir} -G Ninja ${options} -DCMAKE_INSTALL_PREFIX=${install_path}
cmake --build build
cmake --install build
```

Consuming: 

```bash
cmake -S ${srcdir} -B ${builddir} -G Ninja -DCMAKE_PREFIX_PATH=${install_path}
cmake --build build
```

```cmake
find_package(umkacxx)
target_link_libraries($PROJECT PRIVATE umkacxx::library)
```
