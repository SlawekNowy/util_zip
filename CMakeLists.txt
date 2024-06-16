include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

set(PROJ_NAME util_zip)
pr_add_library(${PROJ_NAME} STATIC)

pr_add_dependency(${PROJ_NAME} sharedutils TARGET PUBLIC)
pr_add_dependency(${PROJ_NAME} 7zpp TARGET)
pr_add_dependency(${PROJ_NAME} zip TARGET)

pr_add_headers(${PROJ_NAME} "include/")
pr_add_sources(${PROJ_NAME} "src/")

pr_finalize(${PROJ_NAME})
