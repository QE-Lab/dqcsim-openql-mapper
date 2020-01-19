# Unfortunately OpenQL's CMakeLists aren't exactly designed to be included...
# so we have to repeat a lot of stuff here and hope it doesn't change
# considerably upstream. Ultimately, this makes a target named `openql` to
# link against in the usual way.
find_path(LEMON_SOURCE_ROOT_DIR CMakeLists.txt
    ${PROJECT_SOURCE_DIR}/OpenQL/deps/lemon
    NO_DEFAULT_PATH
    DOC "Location of LEMON source as a CMAKE subproject"
)
if (NOT LEMON_SOURCE_ROOT_DIR)
    message(
        FATAL_ERROR
        "Lemon headers not found. Did you check out recursively?"
    )
endif()
add_subdirectory(${LEMON_SOURCE_ROOT_DIR} OpenQL/deps/lemon)
unset(LEMON_ROOT_DIR CACHE)
unset(LEMON_DIR CACHE)
unset(LEMON_INCLUDE_DIR CACHE)
unset(LEMON_LIBRARY CACHE)
add_library(openql INTERFACE)
set_target_properties(openql PROPERTIES
    INTERFACE_LINK_LIBRARIES
        lemon
    INTERFACE_INCLUDE_DIRECTORIES
        "${LEMON_SOURCE_ROOT_DIR};${PROJECT_BINARY_DIR}/OpenQL/deps/lemon;${PROJECT_SOURCE_DIR}/OpenQL/deps/CLI11/include;${PROJECT_SOURCE_DIR}/OpenQL/src"
)
