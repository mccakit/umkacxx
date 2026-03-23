find_package(PkgConfig REQUIRED)
pkg_check_modules(UMKA REQUIRED IMPORTED_TARGET umka)

file(GLOB umkacxx_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(umkacxx STATIC)
target_sources(umkacxx PUBLIC FILE_SET CXX_MODULES FILES ${umkacxx_srcs})
target_link_libraries(umkacxx PUBLIC PkgConfig::UMKA)

include(GNUInstallDirs)
install(
    TARGETS umkacxx
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    CXX_MODULES_BMI DESTINATION ${CMAKE_INSTALL_LIBDIR}/cxx/bmi
)
configure_file(${CMAKE_SOURCE_DIR}/packaging/umkacxx.pc.in umkacxx.pc @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/umkacxx.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
