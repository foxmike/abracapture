cmake_minimum_required(VERSION 3.22)

project(Abracapture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

include(cmake/Dependencies.cmake)

add_executable(abracapture
    src/app/main.cpp
    src/app/AppController.cpp
    src/app/AppController.hpp
    src/audio/AudioRingBuffer.cpp
    src/audio/AudioRingBuffer.hpp
    src/audio/CaptureMetrics.cpp
    src/audio/CaptureMetrics.hpp
    src/audio/CaptureSession.cpp
    src/audio/CaptureSession.hpp
    src/audio/PipeWireCaptureStream.cpp
    src/audio/PipeWireCaptureStream.hpp
    src/automation/AutoRecordController.cpp
    src/automation/AutoRecordController.hpp
    src/cli/CliApp.cpp
    src/cli/CliApp.hpp
    src/config/AppSettings.cpp
    src/config/AppSettings.hpp
    src/core/AudioFormat.cpp
    src/core/AudioFormat.hpp
    src/export/ExportManager.cpp
    src/export/ExportManager.hpp
    src/pipewire/PipeWireContext.cpp
    src/pipewire/PipeWireContext.hpp
    src/pipewire/PropertyNormalizer.cpp
    src/pipewire/PropertyNormalizer.hpp
    src/pipewire/SourceCatalog.cpp
    src/pipewire/SourceCatalog.hpp
    src/pipewire/SourceDescriptor.cpp
    src/pipewire/SourceDescriptor.hpp
    src/recording/CaptureIntegrity.cpp
    src/recording/CaptureIntegrity.hpp
    src/recording/FlacWriter.cpp
    src/recording/FlacWriter.hpp
    src/recording/OutputPathPolicy.cpp
    src/recording/OutputPathPolicy.hpp
    src/recording/RecordingSink.cpp
    src/recording/RecordingSink.hpp
    src/recording/WavWriter.cpp
    src/recording/WavWriter.hpp
    src/ui/CaptureViewModel.cpp
    src/ui/CaptureViewModel.hpp
    src/ui/SettingsViewModel.cpp
    src/ui/SettingsViewModel.hpp
    src/ui/SourceListModel.cpp
    src/ui/SourceListModel.hpp
    src/ui/WaveformBridge.cpp
    src/ui/WaveformBridge.hpp
)

qt_add_qml_module(abracapture
    URI Abracapture
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/SourceCard.qml
        qml/components/WaveformView.qml
        qml/pages/CapturePage.qml
        qml/pages/SettingsPage.qml
)

target_include_directories(abracapture PRIVATE src)

target_link_libraries(abracapture
    PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        PkgConfig::PIPEWIRE
        PkgConfig::SNDFILE
        PkgConfig::FLAC
)

include(packaging/install.cmake)
