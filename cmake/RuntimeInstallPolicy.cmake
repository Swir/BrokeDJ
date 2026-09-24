# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Swir
#
# Windows release staging uses CMAKE_PROJECT_INCLUDE so this policy is invoked
# after every project() command. BrokeDJ needs JUCE targets to build, but the
# standalone application does not need JUCE's developer SDK install tree.

if(PROJECT_NAME STREQUAL "JUCE")
    # Keep JUCE build/link targets available while suppressing its headers,
    # CMake package and juceaide install rules from BrokeDJ's parent install.
    set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

function(_brokedj_verify_runtime_install_payload)
    # Schedule a final install-time assertion from the BrokeDJ top-level
    # directory. If JUCE changes its install behavior, packaging fails closed
    # instead of publishing a developer SDK inside the portable application.
    install(CODE [[
        file(GLOB _brokedj_juce_developer_payload LIST_DIRECTORIES true
            "${CMAKE_INSTALL_PREFIX}/include/JUCE-*"
            "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-*"
            "${CMAKE_INSTALL_PREFIX}/bin/JUCE-*")
        if(_brokedj_juce_developer_payload)
            list(JOIN _brokedj_juce_developer_payload ", " _brokedj_juce_payload_text)
            message(FATAL_ERROR
                "BrokeDJ runtime staging contains JUCE developer payload: ${_brokedj_juce_payload_text}")
        endif()
    ]])
endfunction()

if(PROJECT_NAME STREQUAL "BrokeDJ" AND PROJECT_IS_TOP_LEVEL)
    # DEFER makes this rule the final top-level install assertion, after
    # dependency subdirectories have registered their install scripts.
    cmake_language(DEFER CALL _brokedj_verify_runtime_install_payload)
endif()
