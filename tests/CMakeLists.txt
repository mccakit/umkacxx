find_package(PkgConfig REQUIRED)
pkg_check_modules(UT REQUIRED IMPORTED_TARGET boost_ut)
add_executable(return_test)
target_sources(return_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/return/test.cpp)
target_link_libraries(return_test PRIVATE umkacxx PkgConfig::UT)
add_custom_command(TARGET return_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/return/test.um
        $<TARGET_FILE_DIR:return_test>/test.um
)
add_test(NAME return_test COMMAND return_test)
