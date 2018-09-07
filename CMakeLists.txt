cmake_minimum_required(VERSION 2.8.4) #this is here because cmake will complain without it, but this CMakeLists.txt might conceivably work for earlier versions of cmake

set(CLANG_ROOT "" CACHE FILEPATH "if not empty, the build system will assume that CLANG_ROOT points to the base path of a (NON-installed) binary release of clang, which will be used both to compile this tool and to get the required libraries")

if (NOT "${CLANG_ROOT}" STREQUAL "")
  #in this case this works more or less like cross-compilation. Just to be on the safe side, add here some typical corss-compilation incantations, adapted to this situation
  set(CLANG_VERSION "5.0.2" CACHE STRING "this should be set to the clang version (default is 5.0.2)")
  set(TOOL_ARCH "x86_64-unknown-linux-gnu" CACHE STRING "this is a descriptor for a neutral, non-optimized architecture")
  set(CMAKE_C_COMPILER "${CLANG_ROOT}/bin/clang")
  set(CMAKE_C_COMPILER_TARGET "${TOOL_ARCH}")
  set(CMAKE_CXX_COMPILER "${CLANG_ROOT}/bin/clang++")
  set(CMAKE_CXX_COMPILER_TARGET "${TOOL_ARCH}")
  set(CLANGPATHS PATHS "${CLANG_ROOT}")
  set(CLANGINCLUDE "${CLANG_ROOT}/include/c++/v1")
  set(CLANGRUNTIMELIBS c++ c++abi)
else()
  set(CLANGPATHS)
  set(CLANGINCLUDE)
  set(CLANGRUNTIMELIBS)
endif()

project(SimpleRefactor)

find_package(LLVM  REQUIRED CONFIG ${CLANGPATHS})
find_package(Clang REQUIRED  ${CLANGPATHS})

set(LLVM_CONFIG_EXE "llvm-config" CACHE FILEPATH "name of llvm-config")

execute_process(
  COMMAND ${LLVM_CONFIG_EXE} --cxxflags
  OUTPUT_VARIABLE LibClang_DEFINITIONS)

add_definitions(${LibClang_DEFINITIONS} "-std=c++11" "-fno-rtti")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANGINCLUDE})

add_executable(simpleRefactor simpleRefactor.cpp)

if (NOT "${CLANG_ROOT}" STREQUAL "")
  link_directories("${CLANG_ROOT}/lib" "${CLANG_ROOT}/lib/clang/${CLANG_VERSION}/lib/linux")
endif()

#for this to succeed, these libraries must be accessible
target_link_libraries(simpleRefactor
  ${CLANGRUNTIMELIBS}
  clangTooling
  clangBasic
  clangASTMatchers
)

add_custom_target(copyfiles ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different grokscrap.py "${CMAKE_BINARY_DIR}/grokscrap.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different refactor.py  "${CMAKE_BINARY_DIR}/refactor.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different examples/test.cpp  "${CMAKE_BINARY_DIR}/examples/test.cpp"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different examples/include/test.hpp  "${CMAKE_BINARY_DIR}/examples/include/test.hpp"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different examples/config.xml  "${CMAKE_BINARY_DIR}/examples/config.xml"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

file(COPY examples DESTINATION "${CMAKE_BINARY_DIR}")

#message(STATUS "EXAMPLES FROM THE COMMAND LINE (execute in the build directory after doing 'make'):")
#message(STATUS "  TEST THE CLANG REFACTORING TOOL:")
#message(STATUS "    ./simpleRefactor --term=UseSpanishLanguage --value=true --overwrite examples/test.cpp -- -I./examples/include")
#message(STATUS "  TEST THE GROK SCRAPER:")
#message(STATUS "    ./grokscrap.py")
#message(STATUS "  TEST THE PYTHON WRAPPER FOR THE REFACTORING TOOL:")
#message(STATUS "    ./refactor.py")
