# соберём тест, используя библиотеку tree_lib
add_executable(test1 test1.cpp)

target_include_directories(test1 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test1 PRIVATE tree_lib)

# добавим тест в CTest
add_test(NAME tree_test1 COMMAND test1)
set_tests_properties(tree_test1 PROPERTIES TIMEOUT 10)
