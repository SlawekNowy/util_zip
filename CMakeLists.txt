
include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

set(PROJ_NAME util_zip)
pr_add_library(${PROJ_NAME} STATIC)

pr_add_dependency(${PROJ_NAME} sharedutils TARGET PUBLIC)
if(WIN32)
    pr_add_dependency(${PROJ_NAME} 7zpp TARGET)
endif()
pr_add_dependency(${PROJ_NAME} zip TARGET)

pr_init_module(${PROJ_NAME})

pr_finalize(${PROJ_NAME})
