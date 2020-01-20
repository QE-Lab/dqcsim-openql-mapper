cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

project(dqcsim-mapper
    VERSION 0.0.1
    DESCRIPTION "DQCsim operator wrapping the OpenQL mapper"
    LANGUAGES CXX
)

# Include DQCsim.
include(FetchContent)
FetchContent_Declare(dqcsim
    GIT_REPOSITORY  https://github.com/mbrobbel/dqcsim.git
    GIT_TAG         master
)
FetchContent_MakeAvailable(dqcsim)

# Include OpenQL.
include(cmake/OpenQL.cmake)

# Use C++11.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main operator executable.
add_executable(
    dqcsopopenql-mapper
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gates.cpp
)
target_include_directories(
    dqcsopopenql-mapper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(dqcsopopenql-mapper dqcsim openql)
