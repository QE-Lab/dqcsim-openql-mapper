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
    GIT_TAG         cmake-install # TODO: change to master once merged!
)
FetchContent_MakeAvailable(dqcsim)

# Include OpenQL.
include(OpenQL.cmake)


add_executable(main main.cpp)
target_link_libraries(main dqcsim openql)
