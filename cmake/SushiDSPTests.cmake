# SushiDSPTests.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.

# Registers the cases of a GoogleTest program by size: the Small_, Medium_ or Large_ prefix of each
# suite gives its label and its timeout per case. The cases of a program run in shards per size,
# <program>.<size>_shard_<i>of<N>.

set(SUSHIDSP_TEST_SIZES "Small_|small|60" "Medium_|medium|300" "Large_|large|900")

# The cases one CTest test runs, per size. A process pays its start-up once, so a case run alone
# is mostly start-up. A shard runs its share of a program's cases in one process: a small case is
# a few milliseconds, so 300 of them take a few seconds; a medium case takes up to 5 s, so 25 keep
# a shard under a minute; a large case takes up to 120 s and runs alone.
set(SUSHIDSP_SHARD_CASES_SMALL 300)
set(SUSHIDSP_SHARD_CASES_MEDIUM 25)
set(SUSHIDSP_SHARD_CASES_LARGE 1)

# Where each shard writes its per-case GoogleTest XML; the time reader reads the folder.
set(SUSHIDSP_GTEST_RESULTS_DIR "${PROJECT_BINARY_DIR}/test_results/gtest")

# Counts the cases of <sources> whose suite opens with <prefix> and sets <out_var> to the count.
# A TEST_P or TYPED_TEST_P counts once however many instances it gets, and each INSTANTIATE_*
# adds one for its instance name, so the count is a floor; a shard with no case passes.
function(sushidsp_count_cases out_var prefix)
    set(count 0)
    foreach(source IN LISTS ARGN)
        file(READ "${source}" text)
        string(REGEX MATCHALL
            "(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)[ \t]*[(][ \t\r\n]*${prefix}[A-Za-z0-9_]*"
            declarations "${text}")
        list(LENGTH declarations declared)
        string(REGEX MATCHALL
            "INSTANTIATE_(TYPED_)?TEST_SUITE_P[ \t]*[(][ \t\r\n]*[A-Za-z0-9_]+[ \t\r\n]*,[ \t\r\n]*${prefix}"
            instances "${text}")
        list(LENGTH instances instantiated)
        math(EXPR count "${count} + ${declared} + ${instantiated}")
    endforeach()
    set(${out_var} ${count} PARENT_SCOPE)
endfunction()

# Registers one CTest test that runs <program> with <filter>, as shard <index> of <shards>.
# GoogleTest splits the filtered cases round-robin by GTEST_TOTAL_SHARDS and GTEST_SHARD_INDEX,
# and the test writes its per-case XML under SUSHIDSP_GTEST_RESULTS_DIR. The remaining arguments
# are the VAR=value pairs of the program's own environment.
function(sushidsp_add_shard_test program name filter labels timeout index shards)
    add_test(NAME ${name}
        COMMAND ${program} --gtest_filter=${filter}
            --gtest_output=xml:${SUSHIDSP_GTEST_RESULTS_DIR}/${name}.xml)
    set_tests_properties(${name} PROPERTIES
        LABELS "${labels}"
        TIMEOUT ${timeout}
        ENVIRONMENT "GTEST_TOTAL_SHARDS=${shards};GTEST_SHARD_INDEX=${index};${ARGN}")
endfunction()

# Registers the cases of <program> of one size in shards, with the size's label and a timeout of
# the case timeout times the cases of a shard. <quarantined_names> is the list of Suite.Case names
# that run apart in one test that also carries `quarantined`; a filter after '-' leaves them out
# of the shards. <size_group> is "<prefix>|<label>|<case timeout>". The remaining arguments are
# "ENVIRONMENT" and the program's environment, then "SOURCES" and the program's sources.
function(sushidsp_register_size_shards program size_group quarantined_names)
    cmake_parse_arguments(ARG "" "" "ENVIRONMENT;SOURCES" ${ARGN})
    string(REPLACE "|" ";" parts "${size_group}")
    list(GET parts 0 prefix)
    list(GET parts 1 label)
    list(GET parts 2 case_timeout)
    string(TOUPPER "${label}" upper)
    sushidsp_count_cases(cases "${prefix}" ${ARG_SOURCES})

    # An instantiated suite is named <instance>/<suite>, so each pattern also carries the slash.
    set(quarantined "")
    set(quarantined_cases 0)
    foreach(name IN LISTS quarantined_names)
        if(name MATCHES "^${prefix}")
            list(APPEND quarantined "${name}" "*/${name}")
            math(EXPR quarantined_cases "${quarantined_cases} + 1")
        endif()
    endforeach()
    set(filter "${prefix}*:*/${prefix}*")
    if(quarantined)
        list(JOIN quarantined ":" quarantined_filter)
        string(APPEND filter "-${quarantined_filter}")
        math(EXPR quarantined_timeout "${case_timeout} * ${quarantined_cases}")
        sushidsp_add_shard_test(${program} ${program}.${label}_quarantined
            "${quarantined_filter}" "${label};quarantined" ${quarantined_timeout} 0 1
            ${ARG_ENVIRONMENT})
        math(EXPR cases "${cases} - ${quarantined_cases}")
    endif()
    if(cases LESS 1)
        return()
    endif()

    math(EXPR shards
        "(${cases} + ${SUSHIDSP_SHARD_CASES_${upper}} - 1) / ${SUSHIDSP_SHARD_CASES_${upper}}")
    math(EXPR shard_cases "(${cases} + ${shards} - 1) / ${shards}")
    math(EXPR timeout "${case_timeout} * ${shard_cases}")
    math(EXPR last "${shards} - 1")
    foreach(index RANGE ${last})
        sushidsp_add_shard_test(${program} ${program}.${label}_shard_${index}of${shards}
            "${filter}" "${label}" ${timeout} ${index} ${shards} ${ARG_ENVIRONMENT})
    endforeach()
endfunction()

# Registers the tests of <program> in shards, one block per size, from SUSHIDSP_TEST_SIZES. The
# program's sources are read from its SOURCES property when it is built. This is the one function
# that registers shards.
function(sushidsp_register_sized_tests program quarantined_names)
    get_target_property(relative_sources ${program} SOURCES)
    get_target_property(source_dir ${program} SOURCE_DIR)
    set(sources "")
    foreach(source IN LISTS relative_sources)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}" NORMALIZE OUTPUT_VARIABLE absolute)
        list(APPEND sources "${absolute}")
    endforeach()
    foreach(group IN LISTS SUSHIDSP_TEST_SIZES)
        sushidsp_register_size_shards(${program} "${group}" "${quarantined_names}"
            ${ARGN} SOURCES ${sources})
    endforeach()
endfunction()
