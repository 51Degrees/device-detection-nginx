cmake_minimum_required(VERSION 3.24)
project(PerformanceTest)

include(ExternalProject)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

ExternalProject_Add(ApacheBench
    GIT_REPOSITORY https://github.com/51degrees/apachebench
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
    GIT_TAG main
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
)


function(download_file url filename)

if(NOT EXISTS ${filename})
    file(DOWNLOAD ${url} ${filename}
        TIMEOUT 60  # seconds
        TLS_VERIFY ON)
endif()

endfunction(download_file)


download_file(
    https://media.githubusercontent.com/media/51Degrees/device-detection-data/master/20000%20User%20Agents.csv
    ./uas.csv
)

add_custom_target(perf ALL
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/runPerf.sh .
	COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.template .
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/runPerf.sh ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.template

    VERBATIM
)

add_dependencies(perf ApacheBench-build)
