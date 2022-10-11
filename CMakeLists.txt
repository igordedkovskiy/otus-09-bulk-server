cmake_minimum_required(VERSION 3.2)

set(PATCH_VERSION "2" CACHE INTERNAL "Patch version")
set(PROJECT_VERSION 0.0.${PATCH_VERSION})

project(bulk-server VERSION ${PROJECT_VERSION})

option(WITH_GTEST "Whether to build google test" ON)
option(GIT_SUBMODULE "Check submodules during build" ON)

configure_file(version.h.in version.h)

find_package(Boost COMPONENTS system REQUIRED)

find_package(Threads REQUIRED)
#list(APPEND EXTRA_LINKS ${CMAKE_THREAD_LIBS_INIT})


add_library(async_lib SHARED
    "async.h"
    "async.cpp"
    "async_impl.hpp"
    "async_impl.cpp"
    "CmdCollector.hpp"
    "CmdCollector.cpp"
    "read_input.hpp"
    "read_input.cpp"
    "generator.hpp"
)
set_target_properties(async_lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(async_lib PRIVATE
    ${CMAKE_BINARY_DIR}
)


add_library(server_lib SHARED
    "retransmittor.hpp"
    "retransmittor.cpp"
    "asio_async_server.hpp"
    "asio_async_server.cpp"
)
set_target_properties(server_lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(server_lib PRIVATE
    ${CMAKE_BINARY_DIR}
    ${Boost_INCLUDE_DIR}
)
target_link_libraries(server_lib PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    async_lib
)


add_executable(${PROJECT_NAME} "main.cpp")
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_BINARY_DIR}
    ${Boost_INCLUDE_DIR}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    server_lib
)


if(WITH_GTEST)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        message(STATUS "GTest is found")
    else()
        find_package(Git REQUIRED)
        if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
            # Update submodules as needed
            message(STATUS "Git is found")
            if(GIT_SUBMODULE)
                message(STATUS "Submodules: add")

                if(WITH_GTEST)
                    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/googletest")
                        execute_process(COMMAND ${GIT_EXECUTABLE}
                                        submodule add https://github.com/google/googletest.git googletest
                                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
                        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                            message(FATAL_ERROR "git submodule add https://github.com/google/googletest.git googletest failed
                                    with ${GIT_SUBMOD_RESULT}, please checkout submodules")
                        else()
                            message(STATUS "GTest submodule added")
                        endif()
                    endif()
                endif()

                message(STATUS "Submodules: update")
                execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                                RESULT_VARIABLE GIT_SUBMOD_RESULT)
                if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                    message(FATAL_ERROR "git submodule update --init --recursive failed
                            with ${GIT_SUBMOD_RESULT}, please checkout submodules")
                else()
                    message(STATUS "Submodules updated")
                endif()
            endif()
        endif()
    endif()
endif()

if(WITH_GTEST)
    if(NOT GTest_FOUND)
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/googletest" "googletest")
        message(STATUS "Add googletest subdirectory")
    endif()
    add_executable(test_async "test.cpp")

    target_include_directories(test_async PRIVATE
        ${GTEST_INCLUDE_DIRS}
#        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    set_target_properties(test_async PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

#    target_compile_definitions(test_print_ip PRIVATE
#        $<$<CONFIG:Debug>:DEBUG_VER>
#        $<$<CONFIG:Release>:RELEASE_VER>
#    )

    target_link_libraries(test_async PRIVATE
        ${GTEST_BOTH_LIBRARIES}
        gtest
        gtest_main
        async_lib
        server_lib
        ${CMAKE_THREAD_LIBS_INIT}
    )

#    target_compile_definitions(test_async PRIVATE "-DUNIT_TEST")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /await
    )
    target_compile_options(async_lib PRIVATE
        /W4 /await
    )
    if(WITH_GTEST)
        target_compile_options(test_async PRIVATE
            /W4 /await
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        "-Wall" "-Wextra" "-Werror" "-pedantic" "-fcoroutines"
    )
    target_compile_options(async_lib PRIVATE
        "-Wall" "-Wextra" "-Werror" "-pedantic" "-fcoroutines"
    )
    target_compile_options(server_lib PRIVATE
        "-Wall" "-Wextra" "-Werror" "-pedantic" "-fcoroutines"
    )
    if(WITH_GTEST)
        target_compile_options(test_async PRIVATE
            "-Wall" "-Wextra" "-Werror" "-pedantic" "-fcoroutines"
        )
    endif()

    if(WIN32)
        target_link_libraries(test_async PRIVATE
            wsock32
            ws2_32
        )
        target_link_libraries(server_lib PRIVATE
            wsock32
            ws2_32
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            wsock32
            ws2_32
        )
    endif()
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT divmy@yandex.ru)
include(CPack)

if(WITH_GTEST)
    include(CTest)
    enable_testing()
    list(APPEND CMAKE_CTEST_ARGUMENTS "--rerun-failed" "--output-on-failure")
    add_test(test test_async)
endif()
