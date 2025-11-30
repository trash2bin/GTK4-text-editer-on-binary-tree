# соберём тест, используя библиотеку tree_lib
add_executable(test1 test1.cpp)

target_include_directories(test1 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test1 PRIVATE tree_lib)

# добавим тест в CTest
add_test(NAME tree_test1 COMMAND test1)
set_tests_properties(tree_test1 PROPERTIES TIMEOUT 10)

# добавляем второй тест (tests/test2.cpp)
add_executable(test2 test2.cpp)

target_include_directories(test2 PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test2 PRIVATE tree_lib)

# регистрируем тест в CTest
add_test(NAME tree_test2 COMMAND test2)
set_tests_properties(tree_test2 PROPERTIES TIMEOUT 10)
