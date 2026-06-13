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


# Obtain the User-Agents evidence file. The copy in the
# device-detection-data sub-module is preferred. Where it is not present,
# or is only a Git LFS pointer file, it is downloaded instead. The size
# check guards against both an LFS pointer file (around 130 bytes) and an
# empty download, either of which would silently degrade the benchmark.
set(UAS_LOCAL
    "${CMAKE_CURRENT_LIST_DIR}/../../device-detection-cxx/device-detection-data/20000 User Agents.csv")
set(UAS_TARGET ${CMAKE_BINARY_DIR}/uas.csv)
set(UAS_MIN_SIZE 100000)

function(check_file_size filename minsize resultvar)
    set(${resultvar} FALSE PARENT_SCOPE)
    if(EXISTS ${filename})
        file(SIZE ${filename} _size)
        if(_size GREATER ${minsize})
            set(${resultvar} TRUE PARENT_SCOPE)
        endif()
    endif()
endfunction(check_file_size)

check_file_size(${UAS_TARGET} ${UAS_MIN_SIZE} UAS_TARGET_OK)
if(NOT UAS_TARGET_OK)
    check_file_size(${UAS_LOCAL} ${UAS_MIN_SIZE} UAS_LOCAL_OK)
    if(UAS_LOCAL_OK)
        message(STATUS "Using User-Agents file from device-detection-data")
        file(COPY_FILE ${UAS_LOCAL} ${UAS_TARGET})
    else()
        message(STATUS "Downloading User-Agents file")
        file(DOWNLOAD
            https://media.githubusercontent.com/media/51Degrees/device-detection-data/master/20000%20User%20Agents.csv
            ${UAS_TARGET}
            TIMEOUT 60  # seconds
            TLS_VERIFY ON)
        check_file_size(${UAS_TARGET} ${UAS_MIN_SIZE} UAS_DOWNLOAD_OK)
        if(NOT UAS_DOWNLOAD_OK)
            message(WARNING
                "The User-Agents file could not be obtained from the "
                "device-detection-data sub-module (use 'git lfs pull') or "
                "downloaded. The device detection benchmark will not use "
                "varied User-Agents.")
        endif()
    endif()
endif()

# Obtain the evidence records file in the same way. This is the same set
# of evidence the other 51Degrees APIs benchmark against, so the device
# detection benchmark sends a full set of headers per request (including
# the Sec-CH-UA client hints) rather than a User-Agent alone. The file is
# copied to a name without spaces so it is simple to pass on a command
# line.
set(EVIDENCE_LOCAL
    "${CMAKE_CURRENT_LIST_DIR}/../../device-detection-cxx/device-detection-data/20000 Evidence Records.yml")
set(EVIDENCE_TARGET ${CMAKE_BINARY_DIR}/evidence.yml)
set(EVIDENCE_MIN_SIZE 1000000)

check_file_size(${EVIDENCE_TARGET} ${EVIDENCE_MIN_SIZE} EVIDENCE_TARGET_OK)
if(NOT EVIDENCE_TARGET_OK)
    check_file_size(${EVIDENCE_LOCAL} ${EVIDENCE_MIN_SIZE} EVIDENCE_LOCAL_OK)
    if(EVIDENCE_LOCAL_OK)
        message(STATUS "Using evidence file from device-detection-data")
        file(COPY_FILE ${EVIDENCE_LOCAL} ${EVIDENCE_TARGET})
    else()
        message(STATUS "Downloading evidence file")
        file(DOWNLOAD
            https://media.githubusercontent.com/media/51Degrees/device-detection-data/master/20000%20Evidence%20Records.yml
            ${EVIDENCE_TARGET}
            TIMEOUT 120  # seconds
            TLS_VERIFY ON)
        check_file_size(${EVIDENCE_TARGET} ${EVIDENCE_MIN_SIZE} EVIDENCE_DOWNLOAD_OK)
        if(NOT EVIDENCE_DOWNLOAD_OK)
            message(WARNING
                "The evidence file could not be obtained from the "
                "device-detection-data sub-module (use 'git lfs pull') or "
                "downloaded. The device detection benchmark will fall back "
                "to varied User-Agents only.")
        endif()
    endif()
endif()

add_custom_target(perf ALL
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/runPerf.sh .
	COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.template .
	COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.ipi.template .
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/runPerf.sh ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.template ${CMAKE_CURRENT_LIST_DIR}/nginx.conf.ipi.template

    VERBATIM
)

add_dependencies(perf ApacheBench-build)
