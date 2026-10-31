set(TEST_SOURCES
    main_test.cpp
    test_database.cpp
    test_storage.cpp
    test_cache.cpp
    test_scheduler.cpp
    test_memory.cpp
    test_deadlock.cpp
    test_synchronization.cpp
    test_clocks.cpp
    test_recovery.cpp
    test_api.cpp
)

add_executable(osvault_tests ${TEST_SOURCES})
target_include_directories(osvault_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/doctest
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/crow
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/asio/include
)

target_link_libraries(osvault_tests PRIVATE osvault_core)

# Register CTest suites
add_test(NAME test_database COMMAND osvault_tests -tc="*database*")
add_test(NAME test_storage COMMAND osvault_tests -tc="*storage*")
add_test(NAME test_cache COMMAND osvault_tests -tc="*cache*")
add_test(NAME test_scheduler COMMAND osvault_tests -tc="*scheduler*")
add_test(NAME test_memory COMMAND osvault_tests -tc="*memory*")
add_test(NAME test_deadlock COMMAND osvault_tests -tc="*deadlock*")
add_test(NAME test_synchronization COMMAND osvault_tests -tc="*synchronization*")
add_test(NAME test_clocks COMMAND osvault_tests -tc="*clocks*")
add_test(NAME test_recovery COMMAND osvault_tests -tc="*recovery*")
add_test(NAME test_api COMMAND osvault_tests -tc="*api*")
