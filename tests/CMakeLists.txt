find_package(PkgConfig REQUIRED)
pkg_check_modules(UT REQUIRED IMPORTED_TARGET boost_ut)

add_executable(return_test)
target_sources(return_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/return/main.cpp)
target_link_libraries(return_test PRIVATE umkacxx PkgConfig::UT)
set_target_properties(return_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/return
)
add_custom_command(TARGET return_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/return/mod.um
        $<TARGET_FILE_DIR:return_test>/mod.um
)
add_test(NAME return_test COMMAND return_test WORKING_DIRECTORY $<TARGET_FILE_DIR:return_test>)

add_library(cppfuncs STATIC)
target_sources(cppfuncs PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/hook/fn.cpp)
target_link_libraries(cppfuncs PRIVATE umkacxx)
set_target_properties(cppfuncs PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hook
)

add_executable(hooktest)
target_sources(hooktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hook/main.cpp)
target_link_libraries(hooktest PRIVATE umkacxx PkgConfig::UT cppfuncs)
set_target_properties(hooktest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hook
)
add_custom_command(TARGET hooktest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/hook/main.um
        $<TARGET_FILE_DIR:hooktest>/main.um
)
add_test(NAME hooktest COMMAND hooktest WORKING_DIRECTORY $<TARGET_FILE_DIR:hooktest>)

add_executable(setter_test)
target_sources(setter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/setter/main.cpp)
target_link_libraries(setter_test PRIVATE umkacxx PkgConfig::UT)
set_target_properties(setter_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/setter
)
add_custom_command(TARGET setter_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/setter/main.um
        $<TARGET_FILE_DIR:setter_test>/main.um
)
add_test(NAME setter_test COMMAND setter_test WORKING_DIRECTORY $<TARGET_FILE_DIR:setter_test>)
