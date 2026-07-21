find_package(umka REQUIRED)
file(GLOB umkacxx_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(umkacxx_library)
target_sources(umkacxx_library PUBLIC FILE_SET CXX_MODULES FILES ${umkacxx_srcs})
target_link_libraries(umkacxx_library PRIVATE umka::library)

set_target_properties(umkacxx_library PROPERTIES EXPORT_NAME "library")
install(
    TARGETS umkacxx_library
    EXPORT umkacxx-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/umkacxx)

install(
    PACKAGE_INFO umkacxx
    EXPORT umkacxx-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cps/umkacxx
    CXX_MODULES_DIRECTORY cxxmodules)
