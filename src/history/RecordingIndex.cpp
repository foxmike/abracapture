#include "RecordingIndex.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace abracapture::history {

QJsonObject RecordingEntry::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("filePath")] = filePath;
    obj[QStringLiteral("format")] = format;
    obj[QStringLiteral("sampleRate")] = static_cast<qint64>(sampleRate);
    obj[QStringLiteral("bitDepth")] = static_cast<qint64>(bitDepth);
    obj[QStringLiteral("channels")] = static_cast<int>(channels);
    obj[QStringLiteral("duration")] = duration;
    obj[QStringLiteral("timestamp")] = timestamp.isValid() ? timestamp.toString(Qt::ISODateWithMs) : QString{};
    obj[QStringLiteral("isNative")] = isNative;
    obj[QStringLiteral("sidecarPath")] = sidecarPath;
    return obj;
}

RecordingEntry RecordingEntry::fromJson(const QJsonObject& json) {
    RecordingEntry entry;
    entry.filePath = json.value(QStringLiteral("filePath")).toString();
    entry.format = json.value(QStringLiteral("format")).toString();
    entry.sampleRate = static_cast<std::uint32_t>(json.value(QStringLiteral("sampleRate")).toInteger(0));
    entry.bitDepth = static_cast<std::uint32_t>(json.value(QStringLiteral("bitDepth")).toInteger(0));
    entry.channels = static_cast<std::uint16_t>(json.value(QStringLiteral("channels")).toInteger(0));
    entry.duration = json.value(QStringLiteral("duration")).toDouble(0.0);

    const QString tsStr = json.value(QStringLiteral("timestamp")).toString();
    if (!tsStr.isEmpty()) {
        entry.timestamp = QDateTime::fromString(tsStr, Qt::ISODateWithMs);
        if (!entry.timestamp.isValid()) {
            entry.timestamp = QDateTime::fromString(tsStr, Qt::ISODate);
        }
    }

    entry.isNative = json.value(QStringLiteral("isNative")).toBool(true);
    entry.sidecarPath = json.value(QStringLiteral("sidecarPath")).toString();
    return entry;
}

RecordingIndex::RecordingIndex(QString indexPath)
    : indexPath_(std::move(indexPath)) {
}

QString RecordingIndex::defaultIndexPath() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        return QStringLiteral("history.json");
    }
    return baseDir + QStringLiteral("/history.json");
}

const QString& RecordingIndex::indexPath() const noexcept {
    return indexPath_;
}

void RecordingIndex::setIndexPath(const QString& path) {
    indexPath_ = path;
}

bool RecordingIndex::load() {
    entries_.clear();

    if (indexPath_.isEmpty()) {
        return false;
    }

    QFile file(indexPath_);
    if (!file.exists()) {
        // Missing file handled gracefully as empty index.
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        // Empty file handled gracefully.
        return true;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // Corrupt file handled gracefully with empty index.
        return false;
    }

    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("recordings")) && root[QStringLiteral("recordings")].isArray()) {
            array = root[QStringLiteral("recordings")].toArray();
        } else if (root.contains(QStringLiteral("entries")) && root[QStringLiteral("entries")].isArray()) {
            array = root[QStringLiteral("entries")].toArray();
        }
    }

    for (const auto& val : array) {
        if (val.isObject()) {
            entries_.append(RecordingEntry::fromJson(val.toObject()));
        }
    }

    sortEntries();
    return true;
}

bool RecordingIndex::save() const {
    if (indexPath_.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(indexPath_);
    const QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return false;
        }
    }

    QSaveFile saveFile(indexPath_);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonArray array;
    for (const auto& entry : entries_) {
        array.append(entry.toJson());
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("recordings")] = array;

    const QJsonDocument doc(root);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    const qint64 written = saveFile.write(bytes);
    if (written != bytes.size()) {
        saveFile.cancelWriting();
        return false;
    }

    return saveFile.commit();
}

bool RecordingIndex::append(const RecordingEntry& entry) {
    entries_.append(entry);
    sortEntries();
    return save();
}

bool RecordingIndex::remove(const qsizetype index) {
    if (index < 0 || index >= entries_.size()) {
        return false;
    }
    entries_.removeAt(index);
    return save();
}

bool RecordingIndex::remove(const int index) {
    return remove(static_cast<qsizetype>(index));
}

bool RecordingIndex::remove(const QString& filePath) {
    bool removed = false;
    for (qsizetype i = entries_.size() - 1; i >= 0; --i) {
        if (entries_[i].filePath == filePath) {
            entries_.removeAt(i);
            removed = true;
        }
    }
    if (removed) {
        return save();
    }
    return false;
}

void RecordingIndex::clear() {
    entries_.clear();
}

QList<RecordingEntry> RecordingIndex::entries() const {
    return entries_;
}

qsizetype RecordingIndex::count() const noexcept {
    return entries_.size();
}

qsizetype RecordingIndex::size() const noexcept {
    return entries_.size();
}

bool RecordingIndex::isEmpty() const noexcept {
    return entries_.isEmpty();
}

std::optional<RecordingEntry> RecordingIndex::at(const qsizetype index) const {
    if (index < 0 || index >= entries_.size()) {
        return std::nullopt;
    }
    return entries_.at(index);
}

void RecordingIndex::sortEntries() {
    std::stable_sort(entries_.begin(), entries_.end(), [](const RecordingEntry& a, const RecordingEntry& b) {
        if (a.timestamp.isValid() && b.timestamp.isValid()) {
            return a.timestamp > b.timestamp; // newest first
        }
        if (a.timestamp.isValid()) {
            return true;
        }
        if (b.timestamp.isValid()) {
            return false;
        }
        return false;
    });
}

} // namespace abracapture::history
