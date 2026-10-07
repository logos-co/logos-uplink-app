cmake_minimum_required(VERSION 3.14)
project(UplinkUiPlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

logos_module(
    NAME uplink_ui
    REP_FILE src/uplink_ui.rep
    SOURCES
        src/UplinkBackend.h
        src/UplinkBackend.cpp
        src/InvitationCode.h
        src/ActivityLog.h
        src/PayoutCode.h
        src/interfaces/Result.h
        src/interfaces/ReferralService.h
        src/interfaces/NodeService.h
        src/lez/LezReferralService.h
        src/lez/LezReferralService.cpp
        src/blockchain/BlockchainNodeService.h
        src/blockchain/BlockchainNodeService.cpp
        src/mock/MockReferralService.h
        src/mock/MockReferralService.cpp
        src/mock/MockNodeService.h
        src/mock/MockNodeService.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
