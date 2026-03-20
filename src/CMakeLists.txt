find_package(PkgConfig REQUIRED)
pkg_check_modules(UMKA REQUIRED IMPORTED_TARGET umka)

file(GLOB umkacxx_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(umkacxx STATIC)
target_sources(umkacxx PUBLIC FILE_SET CXX_MODULES FILES ${umkacxx_srcs})
target_link_libraries(umkacxx PRIVATE PkgConfig::UMKA)
