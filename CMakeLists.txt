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
add_library(brokedj_core STATIC
    src/core/Engine.cpp
    src/core/BeatAnalysis.cpp
    src/core/KeyAnalysis.cpp)
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

    add_executable(brokedj_engine_deck_source_tests tests/EngineDeckSourceTests.cpp)
    target_link_libraries(brokedj_engine_deck_source_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME engine_deck_source COMMAND brokedj_engine_deck_source_tests)
    set_tests_properties(engine_deck_source PROPERTIES TIMEOUT 30)

    add_executable(brokedj_beat_analysis_tests tests/BeatAnalysisTests.cpp)
    target_link_libraries(brokedj_beat_analysis_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME beat_analysis COMMAND brokedj_beat_analysis_tests)
    set_tests_properties(beat_analysis PROPERTIES TIMEOUT 60)

    add_executable(brokedj_key_analysis_tests tests/KeyAnalysisTests.cpp)
    target_link_libraries(brokedj_key_analysis_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME key_analysis COMMAND brokedj_key_analysis_tests)
    set_tests_properties(key_analysis PROPERTIES TIMEOUT 60)

    add_executable(brokedj_performance_deck_owner_tests tests/PerformanceDeckOwnerTests.cpp)
    target_link_libraries(brokedj_performance_deck_owner_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME performance_deck_owner COMMAND brokedj_performance_deck_owner_tests)
    set_tests_properties(performance_deck_owner PROPERTIES TIMEOUT 30)

    add_executable(brokedj_tempo_segment_editor_model_tests tests/TempoSegmentEditorModelTests.cpp)
    target_link_libraries(brokedj_tempo_segment_editor_model_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME tempo_segment_editor_model COMMAND brokedj_tempo_segment_editor_model_tests)
    set_tests_properties(tempo_segment_editor_model PROPERTIES TIMEOUT 30)

    add_executable(brokedj_jog_scratch_controller_tests tests/JogScratchControllerTests.cpp)
    target_link_libraries(brokedj_jog_scratch_controller_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME jog_scratch_controller COMMAND brokedj_jog_scratch_controller_tests)
    set_tests_properties(jog_scratch_controller PROPERTIES TIMEOUT 30)

    add_executable(brokedj_reverse_slip_stream_stress_tests tests/ReverseSlipStreamStressTests.cpp)
    target_link_libraries(brokedj_reverse_slip_stream_stress_tests PRIVATE brokedj_core Threads::Threads)
    add_test(NAME reverse_slip_stream_stress COMMAND brokedj_reverse_slip_stream_stress_tests)
    set_tests_properties(reverse_slip_stream_stress PROPERTIES TIMEOUT 30)
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

    add_library(brokedj_timestretch STATIC
        src/core/TimeStretch.cpp
        src/core/TimeStretchDeck.cpp
        src/core/TimeStretchSourceBridge.cpp
        src/core/TimeStretchDeviceBridge.cpp)
    target_include_directories(brokedj_timestretch PUBLIC src)
    target_link_libraries(brokedj_timestretch PUBLIC brokedj_core PRIVATE signalsmith-stretch)
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

        add_executable(brokedj_timestretch_source_bridge_tests
            tests/TimeStretchSourceBridgeTests.cpp)
        target_link_libraries(brokedj_timestretch_source_bridge_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_source_bridge COMMAND brokedj_timestretch_source_bridge_tests)
        set_tests_properties(time_stretch_source_bridge PROPERTIES TIMEOUT 60)

        add_executable(brokedj_timestretch_source_bridge_realtime_tests
            tests/TimeStretchSourceBridgeRealtimeTests.cpp)
        target_link_libraries(brokedj_timestretch_source_bridge_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_source_bridge_realtime
            COMMAND brokedj_timestretch_source_bridge_realtime_tests)
        set_tests_properties(time_stretch_source_bridge_realtime PROPERTIES TIMEOUT 60)

        add_executable(brokedj_timestretch_device_bridge_tests
            tests/TimeStretchDeviceBridgeTests.cpp)
        target_link_libraries(brokedj_timestretch_device_bridge_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_device_bridge COMMAND brokedj_timestretch_device_bridge_tests)
        set_tests_properties(time_stretch_device_bridge PROPERTIES TIMEOUT 90)

        add_executable(brokedj_timestretch_device_bridge_realtime_tests
            tests/TimeStretchDeviceBridgeRealtimeTests.cpp)
        target_link_libraries(brokedj_timestretch_device_bridge_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_device_bridge_realtime
            COMMAND brokedj_timestretch_device_bridge_realtime_tests)
        set_tests_properties(time_stretch_device_bridge_realtime PROPERTIES TIMEOUT 90)

        add_executable(brokedj_timestretch_engine_bridge_tests
            tests/TimeStretchEngineBridgeTests.cpp)
        target_link_libraries(brokedj_timestretch_engine_bridge_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_engine_bridge COMMAND brokedj_timestretch_engine_bridge_tests)
        set_tests_properties(time_stretch_engine_bridge PROPERTIES TIMEOUT 90)

        add_executable(brokedj_timestretch_engine_bridge_realtime_tests
            tests/TimeStretchEngineBridgeRealtimeTests.cpp)
        target_link_libraries(brokedj_timestretch_engine_bridge_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME time_stretch_engine_bridge_realtime
            COMMAND brokedj_timestretch_engine_bridge_realtime_tests)
        set_tests_properties(time_stretch_engine_bridge_realtime PROPERTIES TIMEOUT 90)

        add_executable(brokedj_deck_playback_selector_tests
            tests/DeckPlaybackSelectorTests.cpp)
        target_link_libraries(brokedj_deck_playback_selector_tests PRIVATE brokedj_timestretch)
        add_test(NAME deck_playback_selector COMMAND brokedj_deck_playback_selector_tests)
        set_tests_properties(deck_playback_selector PROPERTIES TIMEOUT 90)

        add_executable(brokedj_deck_playback_selector_realtime_tests
            tests/DeckPlaybackSelectorRealtimeTests.cpp)
        target_link_libraries(brokedj_deck_playback_selector_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME deck_playback_selector_realtime
            COMMAND brokedj_deck_playback_selector_realtime_tests)
        set_tests_properties(deck_playback_selector_realtime PROPERTIES TIMEOUT 90)

        add_executable(brokedj_engine_keylock_source_tests
            tests/EngineKeyLockSourceTests.cpp)
        target_link_libraries(brokedj_engine_keylock_source_tests PRIVATE brokedj_timestretch)
        add_test(NAME engine_keylock_source COMMAND brokedj_engine_keylock_source_tests)
        set_tests_properties(engine_keylock_source PROPERTIES TIMEOUT 90)

        add_executable(brokedj_engine_keylock_source_realtime_tests
            tests/EngineKeyLockSourceRealtimeTests.cpp)
        target_link_libraries(brokedj_engine_keylock_source_realtime_tests PRIVATE brokedj_timestretch)
        add_test(NAME engine_keylock_source_realtime
            COMMAND brokedj_engine_keylock_source_realtime_tests)
        set_tests_properties(engine_keylock_source_realtime PROPERTIES TIMEOUT 90)

        add_executable(brokedj_keylock_deck_lifecycle_tests
            tests/KeyLockDeckLifecycleTests.cpp)
        target_link_libraries(brokedj_keylock_deck_lifecycle_tests PRIVATE brokedj_timestretch)
        add_test(NAME keylock_deck_lifecycle COMMAND brokedj_keylock_deck_lifecycle_tests)
        set_tests_properties(keylock_deck_lifecycle PROPERTIES TIMEOUT 90)
    endif()
endif()

if(BROKEDJ_BUILD_APP)
    include(FetchContent)

    # SQLite 3.53.4 official amalgamation, released 2026-07-24.
    # Source: https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    # Hash is the official SHA3-256 published by sqlite.org/download.html.
    FetchContent_Declare(sqlite_amalgamation
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
        URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(sqlite_amalgamation)

    set(BROKEDJ_SQLITE_SOURCE_DIR "${sqlite_amalgamation_SOURCE_DIR}")
    if(EXISTS "${sqlite_amalgamation_SOURCE_DIR}/sqlite-amalgamation-3530400/sqlite3.c")
        set(BROKEDJ_SQLITE_SOURCE_DIR
            "${sqlite_amalgamation_SOURCE_DIR}/sqlite-amalgamation-3530400")
    endif()
    if(NOT EXISTS "${BROKEDJ_SQLITE_SOURCE_DIR}/sqlite3.c")
        message(FATAL_ERROR "Pinned SQLite amalgamation did not contain sqlite3.c")
    endif()

    add_library(brokedj_sqlite STATIC
        ${BROKEDJ_SQLITE_SOURCE_DIR}/sqlite3.c)
    target_include_directories(brokedj_sqlite SYSTEM PUBLIC
        ${BROKEDJ_SQLITE_SOURCE_DIR})
    target_compile_definitions(brokedj_sqlite PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_DEFAULT_FOREIGN_KEYS=1
        SQLITE_OMIT_LOAD_EXTENSION=1)
    # Do not apply BrokeDJ's warning policy to vendored third-party C.
    if(MSVC)
        target_compile_options(brokedj_sqlite PRIVATE /w)
    else()
        target_compile_options(brokedj_sqlite PRIVATE -w)
    endif()

    add_library(brokedj_library STATIC
        src/app/LibraryDatabase.cpp
        src/app/LibraryFilePresence.cpp)
    target_include_directories(brokedj_library PUBLIC src)
    target_link_libraries(brokedj_library PUBLIC brokedj_sqlite)
    if(MSVC)
        target_compile_options(brokedj_library PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_library PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
    if(BROKEDJ_SANITIZE AND NOT MSVC)
        target_compile_options(brokedj_library PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(brokedj_library PUBLIC -fsanitize=address,undefined)
    endif()

    # JUCE 9.0.2, released 2026-09-07. Immutable upstream commit, not a floating branch.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 72782788ce18c2d4d760b28e0921d6ffc6431102
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)

    # Keep file decoding/read-ahead and offline track analysis as testable
    # adapters instead of compiling them only as part of the GUI executable.
    add_library(brokedj_decoder STATIC
        src/app/Decoder.cpp
        src/app/TrackAnalysis.cpp
        src/app/PerformanceStateStore.cpp)
    target_include_directories(brokedj_decoder PUBLIC src)
    target_compile_definitions(brokedj_decoder PUBLIC JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_CURL=0)
    target_link_libraries(brokedj_decoder PUBLIC brokedj_core juce::juce_audio_formats)
    if(MSVC)
        target_compile_options(brokedj_decoder PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_decoder PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()

    # Local corpus validator. It is intentionally not an automatic CTest because
    # the repository does not redistribute reference music. The target is built
    # in normal app builds so the manifest-driven validator cannot silently rot.
    add_executable(brokedj_analysis_validator tools/AnalysisValidator.cpp)
    target_link_libraries(brokedj_analysis_validator PRIVATE brokedj_decoder)
    if(MSVC)
        target_compile_options(brokedj_analysis_validator PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_analysis_validator PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()

    if(BUILD_TESTING)
        add_executable(brokedj_decoder_tests tests/DecoderTests.cpp)
        target_link_libraries(brokedj_decoder_tests PRIVATE brokedj_decoder)
        add_test(NAME decoder COMMAND brokedj_decoder_tests)
        set_tests_properties(decoder PROPERTIES TIMEOUT 60)

        add_executable(brokedj_decoder_transport_stress_tests tests/DecoderTransportStressTests.cpp)
        target_link_libraries(brokedj_decoder_transport_stress_tests PRIVATE brokedj_decoder)
        add_test(NAME decoder_transport_stress COMMAND brokedj_decoder_transport_stress_tests)
        set_tests_properties(decoder_transport_stress PROPERTIES TIMEOUT 60)

        add_executable(brokedj_track_analysis_tests tests/TrackAnalysisTests.cpp)
        target_link_libraries(brokedj_track_analysis_tests PRIVATE brokedj_decoder)
        add_test(NAME track_analysis COMMAND brokedj_track_analysis_tests)
        set_tests_properties(track_analysis PROPERTIES TIMEOUT 90)

        add_executable(brokedj_performance_state_store_tests tests/PerformanceStateStoreTests.cpp)
        target_link_libraries(brokedj_performance_state_store_tests PRIVATE brokedj_decoder)
        add_test(NAME performance_state_store COMMAND brokedj_performance_state_store_tests)
        set_tests_properties(performance_state_store PROPERTIES TIMEOUT 30)

        add_executable(brokedj_set_recorder_tests tests/SetRecorderTests.cpp)
        target_include_directories(brokedj_set_recorder_tests PRIVATE src)
        target_compile_definitions(brokedj_set_recorder_tests PRIVATE JUCE_USE_CURL=0)
        target_link_libraries(brokedj_set_recorder_tests PRIVATE juce::juce_audio_formats)
        add_test(NAME set_recorder COMMAND brokedj_set_recorder_tests)
        set_tests_properties(set_recorder PROPERTIES TIMEOUT 30)

        add_executable(brokedj_library_database_tests tests/LibraryDatabaseTests.cpp)
        target_link_libraries(brokedj_library_database_tests PRIVATE brokedj_library)
        add_test(NAME library_database COMMAND brokedj_library_database_tests)
        set_tests_properties(library_database PROPERTIES TIMEOUT 60)

        add_executable(brokedj_library_file_presence_tests tests/LibraryFilePresenceTests.cpp)
        target_link_libraries(brokedj_library_file_presence_tests PRIVATE brokedj_library)
        add_test(NAME library_file_presence COMMAND brokedj_library_file_presence_tests)
        set_tests_properties(library_file_presence PROPERTIES TIMEOUT 60)

        add_executable(brokedj_library_runtime_store_tests tests/LibraryRuntimeStoreTests.cpp)
        target_link_libraries(brokedj_library_runtime_store_tests PRIVATE brokedj_library)
        add_test(NAME library_runtime_store COMMAND brokedj_library_runtime_store_tests)
        set_tests_properties(library_runtime_store PROPERTIES TIMEOUT 60)
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
    if(BROKEDJ_BUILD_TIMESTRETCH_PROTOTYPE)
        target_compile_definitions(BrokeDJ PRIVATE BROKEDJ_TIMESTRETCH_PROTOTYPE=1)
        target_link_libraries(BrokeDJ PRIVATE brokedj_timestretch)
    endif()
    if(MSVC)
        target_compile_options(BrokeDJ PRIVATE /utf-8)
    endif()
    target_link_libraries(BrokeDJ PRIVATE brokedj_decoder brokedj_library juce::juce_audio_utils
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

if(BROKEDJ_BUILD_APP AND BUILD_TESTING)
    # Exercise the real MainComponent asynchronous import boundary with audio
    # initialization disabled. This covers message-thread publication/session
    # state and failed-replacement retention without pretending to qualify a
    # physical audio device or manual drag/drop/file-chooser usability.
    juce_add_console_app(brokedj_native_import_smoke
        PRODUCT_NAME "BrokeDJ native import smoke"
        COMPANY_NAME "Swir")
    juce_generate_juce_header(brokedj_native_import_smoke)
    target_sources(brokedj_native_import_smoke PRIVATE
        tests/NativeImportSmoke.cpp
        src/app/MainComponent.cpp)
    target_include_directories(brokedj_native_import_smoke PRIVATE src)
    target_compile_definitions(brokedj_native_import_smoke PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
    if(MSVC)
        target_compile_options(brokedj_native_import_smoke PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(brokedj_native_import_smoke PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
    target_link_libraries(brokedj_native_import_smoke PRIVATE
        brokedj_decoder
        brokedj_library
        juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME native_import_smoke COMMAND brokedj_native_import_smoke)
    set_tests_properties(native_import_smoke PROPERTIES TIMEOUT 30)
endif()
