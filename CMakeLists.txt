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
    src/export/ExportOutputPlanner.cpp
    src/export/ExportOutputPlanner.hpp
    src/export/ExportManager.cpp
    src/export/ExportManager.hpp
    src/export/ManualExportPlanner.cpp
    src/export/ManualExportPlanner.hpp
    src/export/ExportTypes.cpp
    src/export/ExportTypes.hpp
    src/export/FfmpegArgumentBuilder.cpp
    src/export/FfmpegArgumentBuilder.hpp
    src/history/RecordingIndex.cpp
    src/history/RecordingIndex.hpp
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
    src/ui/ManualExportViewModel.cpp
    src/ui/ManualExportViewModel.hpp
    src/ui/NotificationService.cpp
    src/ui/NotificationService.hpp
    src/ui/RecordingsViewModel.cpp
    src/ui/RecordingsViewModel.hpp
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
        qml/components/ThemedComboBox.qml
        qml/components/WaveformView.qml
        qml/pages/CapturePage.qml
        qml/pages/SettingsPage.qml
        qml/pages/RecordingsPage.qml
        qml/pages/ExportPage.qml
        qml/pages/AboutPage.qml
        qml/theme/AbracaptureTheme.qml
    RESOURCES
        qml/theme/qmldir
)

target_include_directories(abracapture PRIVATE src)

target_link_libraries(abracapture
    PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        PkgConfig::PIPEWIRE
        PkgConfig::SNDFILE
        PkgConfig::FLAC
)

include(CTest)

if(BUILD_TESTING)
    add_executable(test_export
        tests/export/test_export.cpp
        src/export/ExportOutputPlanner.cpp
        src/export/ExportOutputPlanner.hpp
        src/export/ExportManager.cpp
        src/export/ExportManager.hpp
        src/export/ManualExportPlanner.cpp
        src/export/ManualExportPlanner.hpp
        src/export/ExportTypes.cpp
        src/export/ExportTypes.hpp
        src/export/FfmpegArgumentBuilder.cpp
        src/export/FfmpegArgumentBuilder.hpp
        src/recording/OutputPathPolicy.cpp
        src/recording/OutputPathPolicy.hpp
        src/config/AppSettings.cpp
        src/config/AppSettings.hpp
        src/history/RecordingIndex.cpp
        src/history/RecordingIndex.hpp
        src/ui/ManualExportViewModel.cpp
        src/ui/ManualExportViewModel.hpp
        src/ui/SettingsViewModel.cpp
        src/ui/SettingsViewModel.hpp
    )
    target_include_directories(test_export PRIVATE src)
    target_link_libraries(test_export PRIVATE Qt6::Core PkgConfig::SNDFILE)
    add_test(NAME export COMMAND test_export)
endif()

include(packaging/install.cmake)
