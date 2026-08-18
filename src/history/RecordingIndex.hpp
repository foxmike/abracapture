#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <cstdint>
#include <optional>

namespace abracapture::history {

struct DerivedExport {
    std::uint64_t jobId = 0;
    QString format;
    QString outputPath;
    QString state;
    QString error;

    [[nodiscard]] QJsonObject toJson() const;
    static DerivedExport fromJson(const QJsonObject& json);

    bool operator==(const DerivedExport& other) const noexcept {
        return jobId == other.jobId && format == other.format &&
               outputPath == other.outputPath && state == other.state &&
               error == other.error;
    }
};

struct RecordingEntry {
    QString filePath;
    QString format;
    std::uint32_t sampleRate = 0;
    std::uint32_t bitDepth = 0;
    std::uint16_t channels = 0;
    double duration = 0.0;
    QDateTime timestamp;
    bool isNative = true;
    QString sidecarPath;
    QString captureSessionId;
    QString trackTitle;
    QString albumTitle;
    QString applicationName;
    QString mediaName;
    QList<DerivedExport> derivedExports;

    [[nodiscard]] QJsonObject toJson() const;
    static RecordingEntry fromJson(const QJsonObject& json);

    bool operator==(const RecordingEntry& other) const noexcept {
        return filePath == other.filePath &&
               format == other.format &&
               sampleRate == other.sampleRate &&
               bitDepth == other.bitDepth &&
               channels == other.channels &&
               duration == other.duration &&
               timestamp == other.timestamp &&
               isNative == other.isNative &&
               sidecarPath == other.sidecarPath &&
               captureSessionId == other.captureSessionId &&
               trackTitle == other.trackTitle &&
               albumTitle == other.albumTitle &&
               applicationName == other.applicationName &&
               mediaName == other.mediaName &&
               derivedExports == other.derivedExports;
    }

    bool operator!=(const RecordingEntry& other) const noexcept {
        return !(*this == other);
    }
};

class RecordingIndex {
public:
    explicit RecordingIndex(QString indexPath = defaultIndexPath());

    [[nodiscard]] static QString defaultIndexPath();

    [[nodiscard]] const QString& indexPath() const noexcept;
    void setIndexPath(const QString& path);

    [[nodiscard]] bool load();
    [[nodiscard]] bool save() const;
    bool importDirectory(const QString& directoryPath);

    bool append(const RecordingEntry& entry);
    bool update(const RecordingEntry& entry);
    bool remove(qsizetype index);
    bool remove(int index);
    bool remove(const QString& filePath);
    void clear();

    [[nodiscard]] QList<RecordingEntry> entries() const;
    [[nodiscard]] qsizetype count() const noexcept;
    [[nodiscard]] qsizetype size() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] std::optional<RecordingEntry> at(qsizetype index) const;

private:
    void sortEntries();

    QString indexPath_;
    QList<RecordingEntry> entries_;
};

} // namespace abracapture::history
