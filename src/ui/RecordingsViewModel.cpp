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
        externalIndex_->load();
    } else {
        ownedIndex_.load();
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
        {SidecarPathRole, "sidecarPath"}
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
    beginResetModel();
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    idx->load();
    syncEntries();
    if (selectedIndex_ >= entries_.size()) {
        selectedIndex_ = entries_.isEmpty() ? -1 : 0;
        emit selectedIndexChanged();
    }
    endResetModel();
    emit countChanged();
}

QVariantMap RecordingsViewModel::get(const int index) const {
    if (index < 0 || index >= entries_.size()) {
        return {};
    }
    const auto& entry = entries_.at(index);
    const QFileInfo info(entry.filePath);
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
        {QStringLiteral("sidecarPath"), entry.sidecarPath}
    };
}

void RecordingsViewModel::syncEntries() {
    auto* idx = externalIndex_ ? externalIndex_ : &ownedIndex_;
    entries_ = idx->entries();
}
