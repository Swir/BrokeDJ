cmake_minimum_required(VERSION 3.24)
project(BrokeDJ VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BROKEDJ_BUILD_APP "Build the native JUCE application" ON)
option(BROKEDJ_SANITIZE "Enable address and undefined-behavior sanitizers" OFF)
option(BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE "Build the opt-in Signalsmith time-stretch/key-lock prototype and tests" OFF)
include(CTest)
add_library(brokedj_core STATIC src/core/Engine.cpp)
target_include_directories(brokedj_core PUBLIC src)
if(MSVC)
    target_compile_options(brokedj_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(brokedj_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
if(BROKEDJ_SANITIZE AND NOT MSVC)
    target_compile_options(brokedj_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(brokedj_core PUBLIC -fsanitize=address,undefined)
endif()
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(brokedj_core_tests tests/CoreTests.cpp)
    target_link_libraries(brokedj_core_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME core COMMAND brokedj_core_tests)
    set_tests_properties(core PROPERTIES TIMEOUT 30)

    add_executable(brokedj_quality_tests tests/QualityTests.cpp)
    target_link_libraries(brokedj_quality_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME quality COMMAND brokedj_quality_tests)
    set_tests_properties(quality PROPERTIES TIMEOUT 30)

    add_executable(brokedj_render_metrics_tests tests/RenderMetricsTests.cpp)
    target_link_libraries(brokedj_render_metrics_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME render_metrics COMMAND brokedj_render_metrics_tests)
    set_tests_properties(render_metrics PROPERTIES TIMEOUT 30)

    add_executable(brokedj_realtime_contract_tests tests/RealtimeContractTests.cpp)
    target_link_libraries(brokedj_realtime_contract_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME realtime_contract COMMAND brokedj_realtime_contract_tests)
    set_tests_properties(realtime_contract PROPERTIES TIMEOUT 30)

    add_executable(brokedj_stream_cache_tests tests/StreamCacheTests.cpp)
    target_link_libraries(brokedj_stream_cache_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME stream_cache COMMAND brokedj_stream_cache_tests)
    set_tests_properties(stream_cache PROPERTIES TIMEOUT 30)
endif()

if(BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE)
    include(FetchContent)
    # Signalsmith Linear 0.3.1. Resolve the upstream release tag to an immutable commit.
    FetchContent_Declare(signalsmith_linear
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
        GIT_TAG 5668673560146a9cfe38c25315071e3fd68c8317
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(signalsmith_linear)

    # Signalsmith Stretch 1.3.2-era main checkpoint. This remains an opt-in
    # research dependency until BrokeDJ's M2 quality/realtime gates are met.
    FetchContent_Declare(signalsmith_stretch
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
        GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(signalsmith_stretch)

    add_library(brokedj_timestretch STATIC src/core/TimeStretch.cpp src/core/TimeStretchDeck.cpp)
    target_include_directories(brokedj_timestretch PUBLIC src)
    target_link_libraries(brokedj_timestretch PRIVATE signalsmith-stretch)
    if(MSVC)
        target_compile_options(brokedj_timestretch PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_timestretch PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
    if(BROKEDJ_SANITIZE AND NOT MSVC)
        target_compile_options(brokedj_timestretch PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(brokedj_timestretch PUBLIC -fsanitize=address,undefined)
    endif()

    if(BUILD_TESTING)
        add_executable(brokedj_timestretch_tests tests/TimeStretchTests.cpp)
        target_link_libraries(brokedj_timestretch_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch COMMAND brokedj_timestretch_tests)
        set_tests_properties(time_stretch PROPERTIES TIMEOUT 60)

        add_executable(brokedj_timestretch_deck_tests tests/TimeStretchDeckTests.cpp)
        target_link_libraries(brokedj_timestretch_deck_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_deck COMMAND brokedj_timestretch_deck_tests)
        set_tests_properties(time_stretch_deck PROPERTIES TIMEOUT 60)

        add_executable(brokedj_timestretch_realtime_tests tests/TimeStretchRealtimeTests.cpp)
        target_link_libraries(brokedj_timestretch_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_realtime COMMAND brokedj_timestretch_realtime_tests)
        set_tests_properties(time_stretch_realtime PROPERTIES TIMEOUT 60)

        add_executable(brokedj_timestretch_deck_realtime_tests tests/TimeStretchDeckRealtimeTests.cpp)
        target_link_libraries(brokedj_timestretch_deck_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_deck_realtime COMMAND brokedj_timestretch_deck_realtime_tests)
        set_tests_properties(time_stretch_deck_realtime PROPERTIES TIMEOUT 60)
    endif()
endif()

if(BROKEDJ_BUILD_APP)
    include(FetchContent)
    # JUCE 9.0.2, released 2026-09-07. Immutable upstream commit, not a floating branch.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 72782788ce18c2d4d760b28e0921d6ffc6431102
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)

    # Keep file decoding/read-ahead as a testable adapter instead of compiling it
    # only as part of the GUI executable.
    add_library(brokedj_decoder STATIC src/app/Decoder.cpp)
    target_include_directories(brokedj_decoder PUBLIC src)
    target_compile_definitions(brokedj_decoder PUBLIC JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_CURL=0)
    target_link_libraries(brokedj_decoder PUBLIC brokedj_core juce::juce_audio_formats)
    if(MSVC)
        target_compile_options(brokedj_decoder PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_decoder PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()

    if(BUILD_TESTING)
        add_executable(brokedj_decoder_tests tests/DecoderTests.cpp)
        target_link_libraries(brokedj_decoder_tests PRIVATE brokedj_decoder)
        add_test(NAME decoder COMMAND brokedj_decoder_tests)
        set_tests_properties(decoder PROPERTIES TIMEOUT 60)
    endif()

    juce_add_gui_app(BrokeDJ PRODUCT_NAME "BrokeDJ" COMPANY_NAME "Swir"
        BUNDLE_ID "io.github.swir.brokedj"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
    juce_generate_juce_header(BrokeDJ)
    target_sources(BrokeDJ PRIVATE src/app/Main.cpp src/app/MainComponent.cpp)
    target_compile_definitions(BrokeDJ PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:BrokeDJ,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
    if(MSVC)
        target_compile_options(BrokeDJ PRIVATE /utf-8)
    endif()
    target_link_libraries(BrokeDJ PRIVATE brokedj_decoder juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    install(TARGETS BrokeDJ RUNTIME DESTINATION . BUNDLE DESTINATION .)
    install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION .)
    install(FILES "${juce_SOURCE_DIR}/LICENSE.md" DESTINATION licenses RENAME JUCE-LICENSE.md)
    install(FILES "${juce_SOURCE_DIR}/JUCE.spdx.json" DESTINATION licenses)
    set(CPACK_PACKAGE_NAME BrokeDJ)
    set(CPACK_PACKAGE_VENDOR Swir)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_FILE_NAME "BrokeDJ-${PROJECT_VERSION}-development-${CMAKE_SYSTEM_NAME}")
    include(CPack)
endif()
