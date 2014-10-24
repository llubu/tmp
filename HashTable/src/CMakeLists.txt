project (HTable)

include_directories(${PROJECT_SOURCE_DIR}/../include)

# Configure a header file to pass some of the CMake settings
# to the source code
#configure_file (
#  "${PROJECT_SOURCE_DIR}/../include/TutorialConfig.h.in"
#  "${PROJECT_BINARY_DIR}/TutorialConfig.h"
#)

add_library(HashTable ./lru_table.c)
