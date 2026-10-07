cmake_minimum_required(VERSION 3.14)
project(UplinkUiTests LANGUAGES CXX)

include(LogosTest)

logos_test(
    NAME uplink_ui_tests
    MODULE_SOURCES
        ../src/mock/MockReferralService.cpp
        ../src/mock/MockNodeService.cpp
    TEST_SOURCES
        main.cpp
        test_mock_referral.cpp
    EXTRA_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
