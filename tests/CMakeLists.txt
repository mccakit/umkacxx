find_package(ut REQUIRED)
set_target_properties(ut::library PROPERTIES CXX_MODULE_STD ON)

add_executable(return_test)
target_sources(return_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/return/main.cpp)
target_link_libraries(return_test PRIVATE umkacxx_library ut::library)
set_target_properties(
    return_test
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/return)
add_custom_command(
    TARGET return_test
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/return/mod.um
        $<TARGET_FILE_DIR:return_test>/mod.um)
add_test(
    NAME return_test
    COMMAND return_test
    WORKING_DIRECTORY $<TARGET_FILE_DIR:return_test>)

add_library(hooks STATIC)
target_sources(
hooks
    PUBLIC
    FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/hook/fn.cpp)
target_link_libraries(hooks PRIVATE umkacxx_library)

add_executable(hooktest)
target_sources(hooktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hook/main.cpp)
target_link_libraries(hooktest PRIVATE umkacxx_library ut::library hooks)
set_target_properties(hooktest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hook
)
add_custom_command(TARGET hooktest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/hook/main.um
        $<TARGET_FILE_DIR:hooktest>/main.um
)
add_test(NAME hooktest COMMAND hooktest WORKING_DIRECTORY
$<TARGET_FILE_DIR:hooktest>)
