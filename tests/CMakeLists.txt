cmake_minimum_required(VERSION 3.14)
project(UplinkUiTests LANGUAGES CXX)

include(LogosTest)

# The backend runs against the mock services, so the real ones are stand-ins
# (fakes/RealServices.cpp) and the SDK's plugin context is a double (fakes/).
logos_test(
    NAME uplink_ui_tests
    MODULE_SOURCES
        ../src/UplinkBackend.cpp
        ../src/mock/MockReferralService.cpp
        ../src/mock/MockNodeService.cpp
        fakes/RealServices.cpp
    TEST_SOURCES
        main.cpp
        test_mock_referral.cpp
        test_backend.cpp
    EXTRA_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

find_package(Qt6 REQUIRED COMPONENTS RemoteObjects)
qt6_add_repc_sources(uplink_ui_tests ../src/uplink_ui.rep)
target_include_directories(uplink_ui_tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
