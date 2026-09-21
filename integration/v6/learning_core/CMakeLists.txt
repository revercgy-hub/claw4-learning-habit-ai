cmake_minimum_required(VERSION 3.20)
project(claw4_v6_learning_core LANGUAGES CXX)

# Migration qualification only. M0/M1 device images must not link this target.
get_filename_component(CLAW4_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CORE "${CLAW4_ROOT}/firmware/main")
add_library(claw4_learning_core STATIC
    "${CORE}/learning_domain/reducer.cpp"
    "${CORE}/application/coordinator.cpp"
    "${CORE}/sync/outbox_core.cpp"
    "${CORE}/sync/wire_codec.cpp"
    "${CORE}/sync/backend_client.cpp"
    "${CORE}/sync/learning_backend_session.cpp"
    "${CORE}/sync/sync_executor.cpp"
    "${CORE}/sync/single_flight_http_transport.cpp"
    "${CORE}/sync/scheduled_http_transport.cpp"
    "${CORE}/interaction/dispatcher.cpp"
    "${CORE}/interaction/interaction_arbiter.cpp"
    "${CORE}/time/time_authority.cpp"
    "${CORE}/reminder/reminder_core.cpp"
    "${CORE}/ui/presenters.cpp")
target_compile_features(claw4_learning_core PUBLIC cxx_std_17)
target_include_directories(claw4_learning_core PUBLIC "${CORE}")
if(MSVC)
    target_compile_options(claw4_learning_core PRIVATE /W4 /WX)
else()
    target_compile_options(claw4_learning_core PRIVATE -Wall -Wextra -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS domain/domain_reducer sync/outbox_core application/coordinator ui/presenter)
        string(REPLACE "/" "_" name "${suite}")
        add_executable(${name} "${CLAW4_ROOT}/firmware/tests/unit/${suite}_tests.cpp")
        target_include_directories(${name} PRIVATE "${CLAW4_ROOT}/firmware/tests")
        target_link_libraries(${name} PRIVATE claw4_learning_core)
        add_test(NAME ${name} COMMAND ${name})
    endforeach()
endif()
