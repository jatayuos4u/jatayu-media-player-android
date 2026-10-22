cmake_minimum_required(VERSION 3.21)
project(JatayuMediaPlayerAndroid VERSION 1.0 LANGUAGES CXX)

add_compile_definitions(JATAYU_APP_VERSION="${PROJECT_VERSION}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11.3 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Multimedia Svg)
qt_standard_project_setup(REQUIRES 6.8)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.6)
    qt_policy(SET QTP0002 NEW)
endif()

qt_add_executable(jatayu-media-player-android
    android/src/InterfaceLanguage.cpp android/src/InterfaceLanguage.h
    android/src/PlaybackDecks.cpp android/src/PlaybackDecks.h
    android/src/PlaybackSession.cpp android/src/PlaybackSession.h
    android/src/MediaSource.cpp android/src/MediaSource.h
    android/src/main.cpp
    android/src/MediaStore.h android/src/MediaStore.cpp
    android/src/ArtworkProvider.h android/src/ArtworkProvider.cpp
    android/src/AudioVisualizer.h android/src/AudioVisualizer.cpp
    android/src/SoundStudio.h android/src/StudioOutput.cpp android/src/StudioOutput.h android/src/SoundStudio.cpp android/src/SoundDsp.h android/src/SoundDsp.cpp
)

set_source_files_properties(android/qml/jatayu-media-player.svg PROPERTIES QT_RESOURCE_ALIAS jatayu-media-player.svg)

foreach(qml_file VolumeControl Main Icon ActionButton MediaCard VideoSurface StudyPanel AboutPage MusicVisualization MusicFullscreen SoundStudioPanel)
    set_source_files_properties(android/qml/${qml_file}.qml PROPERTIES QT_RESOURCE_ALIAS ${qml_file}.qml)
endforeach()

qt_add_qml_module(jatayu-media-player-android
    URI Jatayu.Android
    VERSION 1.0
    QML_FILES android/qml/VolumeControl.qml android/qml/Main.qml android/qml/Icon.qml
        android/qml/ActionButton.qml android/qml/MediaCard.qml android/qml/VideoSurface.qml android/qml/StudyPanel.qml android/qml/AboutPage.qml android/qml/MusicVisualization.qml android/qml/MusicFullscreen.qml android/qml/SoundStudioPanel.qml
    RESOURCES android/qml/jatayu-media-player.svg
)

if(ANDROID)
    if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "This Android build supports arm64-v8a only")
    endif()
    set_target_properties(jatayu-media-player-android PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/package"
        QT_ANDROID_PACKAGE_NAME "io.github.jatayuos4u.jatayumediaplayer"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 19
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()

target_link_libraries(jatayu-media-player-android PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Multimedia Qt6::Svg
)

if(ANDROID)
    target_compile_options(jatayu-media-player-android PRIVATE -fstack-protector-strong)
    target_link_options(jatayu-media-player-android PRIVATE -Wl,-z,relro,-z,now,-z,noexecstack)
endif()

option(JATAYU_BUILD_TESTS "Build host persistence tests" OFF)
if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(media-store-test tests/MediaStoreTest.cpp android/src/MediaStore.cpp android/src/MediaStore.h)
    target_include_directories(media-store-test PRIVATE android/src)
    target_link_libraries(media-store-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME media-store COMMAND media-store-test)
    qt_add_executable(media-identity-test tests/MediaIdentityTest.cpp android/src/MediaStore.cpp android/src/MediaStore.h)
    target_include_directories(media-identity-test PRIVATE android/src)
    target_link_libraries(media-identity-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME media-identity COMMAND media-identity-test)
endif()
if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    qt_add_executable(ui-smoke-test android/src/InterfaceLanguage.cpp android/src/InterfaceLanguage.h android/src/PlaybackDecks.cpp android/src/PlaybackDecks.h android/src/PlaybackSession.cpp android/src/PlaybackSession.h android/src/MediaSource.cpp android/src/MediaSource.h tests/UiSmokeTest.cpp android/src/MediaStore.cpp
        android/src/MediaStore.h android/src/ArtworkProvider.cpp android/src/ArtworkProvider.h
        android/src/AudioVisualizer.cpp android/src/AudioVisualizer.h
        android/src/StudioOutput.cpp android/src/StudioOutput.h android/src/SoundStudio.cpp android/src/SoundStudio.h android/src/SoundDsp.cpp android/src/SoundDsp.h)
    target_include_directories(ui-smoke-test PRIVATE android/src)
    target_compile_definitions(ui-smoke-test PRIVATE JATAYU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ui-smoke-test PRIVATE Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Multimedia Qt6::Svg)
    add_test(NAME ui-smoke COMMAND ui-smoke-test)
    set_tests_properties(ui-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    qt_add_executable(audio-visualizer-test tests/AudioVisualizerTest.cpp android/src/AudioVisualizer.cpp android/src/AudioVisualizer.h)
    target_include_directories(audio-visualizer-test PRIVATE android/src)
    target_link_libraries(audio-visualizer-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME audio-visualizer COMMAND audio-visualizer-test)
    qt_add_executable(security-input-test tests/SecurityInputTest.cpp android/src/ArtworkProvider.cpp android/src/ArtworkProvider.h)
    target_include_directories(security-input-test PRIVATE android/src)
    target_link_libraries(security-input-test PRIVATE Qt6::Test Qt6::Quick)
    add_test(NAME security-input COMMAND security-input-test)
    set_tests_properties(security-input PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(media-decoder-safety-test tests/MediaDecoderSafetyTest.cpp)
    target_link_libraries(media-decoder-safety-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME media-decoder-safety COMMAND media-decoder-safety-test)
endif()


if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    add_executable(sound-dsp-test tests/SoundDspTest.cpp android/src/SoundDsp.cpp)
    target_include_directories(sound-dsp-test PRIVATE android/src)
    target_compile_options(sound-dsp-test PRIVATE -UNDEBUG)
    add_test(NAME sound-dsp COMMAND sound-dsp-test)
    qt_add_executable(sound-studio-test tests/SoundStudioTest.cpp android/src/StudioOutput.cpp android/src/StudioOutput.h android/src/SoundStudio.cpp android/src/SoundDsp.cpp android/src/AudioVisualizer.cpp)
    target_include_directories(sound-studio-test PRIVATE android/src)
    target_link_libraries(sound-studio-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME sound-studio COMMAND sound-studio-test)
endif()

if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    qt_add_executable(media-source-test tests/MediaSourceTest.cpp android/src/MediaSource.cpp)
    target_include_directories(media-source-test PRIVATE android/src)
    target_link_libraries(media-source-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME media-source COMMAND media-source-test)
endif()

if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    qt_add_executable(playback-session-test tests/PlaybackSessionTest.cpp android/src/PlaybackSession.cpp android/src/PlaybackSession.h)
    target_include_directories(playback-session-test PRIVATE android/src)
    target_link_libraries(playback-session-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME playback-session COMMAND playback-session-test)
    find_package(Java 17 COMPONENTS Development)
    if(Java_FOUND)
        add_test(NAME playback-policy COMMAND ${CMAKE_COMMAND}
            -DJAVA_COMPILER=${Java_JAVAC_EXECUTABLE} -DJAVA_RUNTIME=${Java_JAVA_EXECUTABLE}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/policy-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunPlaybackPolicy.cmake)
    endif()
endif()

qt_add_resources(jatayu-media-player-android interface-translations PREFIX "/" FILES android/i18n/hi.json)
if(TARGET ui-smoke-test)
    qt_add_resources(ui-smoke-test interface-translations-test PREFIX "/" FILES android/i18n/hi.json)
endif()
if(JATAYU_BUILD_TESTS AND NOT ANDROID)
    qt_add_executable(playback-decks-test tests/PlaybackDecksTest.cpp android/src/PlaybackDecks.cpp android/src/PlaybackDecks.h android/src/PlaybackSession.cpp android/src/PlaybackSession.h android/src/MediaSource.cpp android/src/MediaSource.h android/src/AudioVisualizer.cpp android/src/AudioVisualizer.h android/src/SoundStudio.cpp android/src/SoundStudio.h android/src/StudioOutput.cpp android/src/StudioOutput.h android/src/SoundDsp.cpp android/src/SoundDsp.h)
    target_include_directories(playback-decks-test PRIVATE android/src)
    target_link_libraries(playback-decks-test PRIVATE Qt6::Test Qt6::Multimedia)
    add_test(NAME playback-decks COMMAND playback-decks-test)
    qt_add_executable(interface-language-test tests/InterfaceLanguageTest.cpp android/src/InterfaceLanguage.cpp android/src/InterfaceLanguage.h)
    target_include_directories(interface-language-test PRIVATE android/src)
    target_link_libraries(interface-language-test PRIVATE Qt6::Test Qt6::Qml)
    qt_add_resources(interface-language-test hindi-test PREFIX "/" FILES android/i18n/hi.json)
    add_test(NAME interface-language COMMAND interface-language-test)
endif()

