cmake_minimum_required(VERSION 2.8.4)
project(SimpleRefactor)

find_package(LLVM  REQUIRED CONFIG)
find_package(Clang REQUIRED)

set(LLVM_CONFIG_EXE "llvm-config-6.0" CACHE FILEPATH "name of llvm-config")
execute_process(
  COMMAND ${LLVM_CONFIG_EXE} --cxxflags
  OUTPUT_VARIABLE LibClang_DEFINITIONS)

add_definitions(${LibClang_DEFINITIONS} "-fno-rtti" "-Wno-class-memaccess")

include_directories(${LLVM_INCLUDE_DIRS})

add_executable(simpleRefactor simpleRefactor.cpp)

#for this to succeed, these libraries must be accessible
target_link_libraries(simpleRefactor
  clangTooling
  clangBasic
  clangASTMatchers
)

add_custom_target(copyfiles ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different grokscrap.py "${CMAKE_BINARY_DIR}/grokscrap.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different refactor.py  "${CMAKE_BINARY_DIR}/refactor.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different examples/test.cpp  "${CMAKE_BINARY_DIR}/examples/test.cpp"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different examples/include/test.hpp  "${CMAKE_BINARY_DIR}/examples/include/test.hpp"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

file(COPY examples DESTINATION "${CMAKE_BINARY_DIR}")

#message(STATUS "EXAMPLES FROM THE COMMAND LINE (execute in the build directory after doing 'make'):")
#message(STATUS "  TEST THE CLANG REFACTORING TOOL:")
#message(STATUS "    ./simpleRefactor --term=UseSpanishLanguage --value=true examples/test.cpp -- -I./examples/include")
#message(STATUS "  TEST THE GROK SCRAPER:")
#message(STATUS "    ./grokscrap.py")
#message(STATUS "  TEST THE PYTHON WRAPPER FOR THE REFACTORING TOOL:")
#message(STATUS "    ./refactor.py")
