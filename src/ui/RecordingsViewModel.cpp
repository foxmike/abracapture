#include "RecordingsViewModel.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

RecordingsViewModel::RecordingsViewModel(abracapture::history::RecordingIndex* index,
                                         QObject* parent)
    : QAbstractListModel(parent)
    , externalIndex_(index) {
    if (externalIndex_) {
        static_cast<void>(externalIndex_->load());
    } else {
        static_cast<void>(ownedIndex_.load());
    }
    syncEntries();
}

RecordingsViewModel::RecordingsViewModel(QObject* parent)
    : RecordingsViewModel(nullptr, parent) {
}

int RecordingsViewModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(entries_.size());
}

QVariant RecordingsViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return {};
    }

    const auto& entry = entries_.at(index.row());
    switch (role) {
    case FilePathRole:
        return entry.filePath;
    case FileNameRole:
    case Qt::DisplayRole: {
        const QFileInfo info(entry.filePath);
        return info.fileName().isEmpty() ? entry.filePath : info.fileName();
    }
    case FormatRole:
        return entry.format;
    case SampleRateRole:
        return static_cast<quint32>(entry.sampleRate);
    case BitDepthRole:
        return static_cast<quint32>(entry.bitDepth);
    case ChannelsRole:
        return static_cast<int>(entry.channels);
    case DurationRole:
        return entry.duration;
    case TimestampRole:
        return entry.timestamp;
    case IsNativeRole:
        return entry.isNative;
    case SidecarPathRole:
        return entry.sidecarPath;
    case CaptureSessionIdRole:
        return entry.captureSessionId;
    case DerivedExportsRole: {
        QVariantList exports;
        for (const auto& exportRecord : entry.derivedExports) {
            exports.append(QVariantMap{
                {QStringLiteral("jobId"), static_cast<qulonglong>(exportRecord.jobId)},
                {QStringLiteral("format"), exportRecord.format},
                {QStringLiteral("outputPath"), exportRecord.outputPath},
                {QStringLiteral("state"), exportRecord.state},
                {QStringLiteral("error"), exportRecord.error},
            });
        }
        return exports;
    }
    case TrackTitleRole:
        return entry.trackTitle;
    case AlbumTitleRole:
        return entry.albumTitle;
    case ApplicationNameRole:
        return entry.applicationName;
    case MediaNameRole:
        return entry.mediaName;
    default:
        return {};
    }
}

QHash<int, QByteArray> RecordingsViewModel::roleNames() const {
    return {
        {FilePathRole, "filePath"},
        {FileNameRole, "fileName"},
        {FormatRole, "format"},
        {SampleRateRole, "sampleRate"},
        {BitDepthRole, "bitDepth"},
        {ChannelsRole, "channels"},
        {DurationRole, "duration"},
        {TimestampRole, "timestamp"},
        {IsNativeRole, "isNative"},
        {SidecarPathRole, "sidecarPath"},
        {CaptureSessionIdRole, "captureSessionId"},
        {DerivedExportsRole, "derivedExports"},
        {TrackTitleRole, "trackTitle"},
        {AlbumTitleRole, "albumTitle"},
        {ApplicationNameRole, "applicationName"},
        {MediaNameRole, "mediaName"}
    };
}

bool RecordingsViewModel::isEmpty() const noexcept {
    return entries_.isEmpty();
}

int RecordingsViewModel::selectedIndex() const noexcept {
    return selectedIndex_;
}

void RecordingsViewModel::setSelectedIndex(const int index) {
    const int target = (index >= 0 && index < entries_.size()) ? index : -1;
    if (selectedIndex_ != target) {
        selectedIndex_ = target;
        emit selectedIndexChanged();
    }
}

QVariantMap RecordingsViewModel::selectedEntry() const {
    return get(selectedIndex_);
}

abracapture::history::RecordingIndex* RecordingsViewModel::recordingIndex() const noexcept {
    return externalIndex_ ? externalIndex_ : const_cast<abracapture::history::RecordingIndex*>(&ownedIndex_);
}

void RecordingsViewModel::setRecordingIndex(abracapture::history::RecordingIndex* index) {
    if (externalIndex_ != index) {
        externalIndex_ = index;
        refreshEntries();
    }
}

bool RecordingsViewModel::revealInFileManager(const int index) {
    if (index < 0 || index >= entries_.size()) {
        return false;
    }
    const auto& entry = entries_.at(index);
    if (entry.filePath.isEmpty()) {
        return false;
    }
    const QFileInfo fileInfo(entry.filePath);
    const QDir parentDir = fileInfo.dir();
    if (!parentDir.exists()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parentDir.absolutePath()));
}

bool RecordingsViewModel::playEntry(const int index) {
    if (index < 0 || index >= entries_.size()) return false;
    const QString path = entries_.at(index).filePath;
    if (!QFileInfo::exists(path)) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

int RecordingsViewModel::exportSelectionCount() const noexcept {
    return exportSelectedPaths_.size();
}

bool RecordingsViewModel::isSelectedForExport(const int index) const {
    return index >= 0 && index < entries_.size() &&
           exportSelectedPaths_.contains(entries_.at(index).filePath);
}

void RecordingsViewModel::toggleExportSelection(const int index) {
    if (index < 0 || index >= entries_.size()) return;
    const QString path = entries_.at(index).filePath;
    if (exportSelectedPaths_.contains(path)) exportSelectedPaths_.remove(path);
    else exportSelectedPaths_.insert(path);
    emit exportSelectionChanged();
}

void RecordingsViewModel::clearExportSelection() {
    if (exportSelectedPaths_.isEmpty()) return;
    exportSelectedPaths_.clear();
    emit exportSelectionChanged();
}

void RecordingsViewModel::selectAllForExport() {
    QSet<QString> allPaths;
    for (const auto& entry : entries_) {
        if (!entry.filePath.isEmpty()) allPaths.insert(entry.filePath);
    }
    if (exportSelectedPaths_ == allPaths) return;
    exportSelectedPaths_ = std::move(allPaths);
    emit exportSelectionChanged();
}

QVariantList RecordingsViewModel::exportSelectedEntries() const {
    QVariantList selected;
    for (int index = 0; index < entries_.size(); ++index) {
        if (exportSelectedPaths_.contains(entries_.at(index).filePath)) selected.append(get(index));
    }
    return selected;
}

QString RecordingsViewModel::operationStatus() const { return operationStatus_; }

bool RecordingsViewModel::removeEntry(const int index) {
    if (index < 0 || index >= entries_.size()) {
        return false;
    }
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    const bool success = idx->remove(index);
    if (success) {
        refreshEntries();
    }
    return success;
}

void RecordingsViewModel::refreshEntries() {
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    if (recordingsDirectory_.isEmpty() || !idx->importDirectory(recordingsDirectory_)) {
        static_cast<void>(idx->load());
    }
    rebuildModel();
}

void RecordingsViewModel::rebuildModel() {
    beginResetModel();
    syncEntries();
    QSet<QString> knownPaths;
    for (const auto& entry : entries_) knownPaths.insert(entry.filePath);
    const QSet<QString> retained = exportSelectedPaths_ & knownPaths;
    if (retained != exportSelectedPaths_) {
        exportSelectedPaths_ = retained;
        emit exportSelectionChanged();
    }
    if (selectedIndex_ >= entries_.size()) {
        selectedIndex_ = entries_.isEmpty() ? -1 : 0;
        emit selectedIndexChanged();
    }
    endResetModel();
    emit countChanged();
}

bool RecordingsViewModel::importDirectory(const QString& directoryPath) {
    recordingsDirectory_ = QDir(directoryPath).absolutePath();
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    const bool success = idx->importDirectory(directoryPath);
    rebuildModel();
    return success;
}

QVariantMap RecordingsViewModel::get(const int index) const {
    if (index < 0 || index >= entries_.size()) {
        return {};
    }
    const auto& entry = entries_.at(index);
    const QFileInfo info(entry.filePath);
    QVariantList derivedExports;
    for (const auto& exportRecord : entry.derivedExports) {
        derivedExports.append(QVariantMap{
            {QStringLiteral("jobId"), QVariant::fromValue<qulonglong>(exportRecord.jobId)},
            {QStringLiteral("format"), exportRecord.format},
            {QStringLiteral("outputPath"), exportRecord.outputPath},
            {QStringLiteral("state"), exportRecord.state},
            {QStringLiteral("error"), exportRecord.error}
        });
    }
    return {
        {QStringLiteral("filePath"), entry.filePath},
        {QStringLiteral("fileName"), info.fileName().isEmpty() ? entry.filePath : info.fileName()},
        {QStringLiteral("format"), entry.format},
        {QStringLiteral("sampleRate"), static_cast<quint32>(entry.sampleRate)},
        {QStringLiteral("bitDepth"), static_cast<quint32>(entry.bitDepth)},
        {QStringLiteral("channels"), static_cast<int>(entry.channels)},
        {QStringLiteral("duration"), entry.duration},
        {QStringLiteral("timestamp"), entry.timestamp},
        {QStringLiteral("isNative"), entry.isNative},
        {QStringLiteral("sidecarPath"), entry.sidecarPath},
        {QStringLiteral("captureSessionId"), entry.captureSessionId},
        {QStringLiteral("trackTitle"), entry.trackTitle},
        {QStringLiteral("albumTitle"), entry.albumTitle},
        {QStringLiteral("applicationName"), entry.applicationName},
        {QStringLiteral("mediaName"), entry.mediaName},
        {QStringLiteral("derivedExports"), derivedExports}
    };
}

void RecordingsViewModel::syncEntries() {
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    entries_ = idx->entries();
}
