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

add_library(cppfuncs STATIC)
target_sources(cppfuncs PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/hook/cppfuncs.cpp)
target_link_libraries(cppfuncs PRIVATE umkacxx)
add_executable(hooktest)
target_sources(hooktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hook/hooktest.cpp)
target_link_libraries(hooktest PRIVATE umkacxx PkgConfig::UT cppfuncs)
add_custom_command(TARGET hooktest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/hook/hooktest.um
        $<TARGET_FILE_DIR:hooktest>/hooktest.um
)

add_test(NAME hooktest COMMAND hooktest)
