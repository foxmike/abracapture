#include "FfmpegCapabilities.hpp"

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

namespace abracapture::exporting {

namespace {

QString locateFfmpeg() {
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return found.isEmpty() ? QStringLiteral("ffmpeg") : found;
}

QSet<QString> parseListOutput(const QByteArray& output) {
    QSet<QString> result;
    const QString text = QString::fromLocal8Bit(output);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('='))) {
            continue;
        }
        const QStringList parts = trimmed.split(QChar::Space, Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            result.insert(parts.at(1).trimmed());
        }
    }
    return result;
}

QString parseVersion(const QByteArray& output) {
    const QString text = QString::fromLocal8Bit(output);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("ffmpeg version"))) {
            const QStringList parts = line.split(QChar::Space, Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                return parts.at(2);
            }
        }
    }
    return {};
}

} // namespace

FfmpegCapabilities::FfmpegCapabilities(QObject* const parent)
    : QObject(parent) {}

FfmpegCapabilities::ProbeResult FfmpegCapabilities::executeProbe() {
    ProbeResult result;
    result.ffmpegPath = locateFfmpeg();

    QProcess versionProcess;
    versionProcess.start(result.ffmpegPath, {QStringLiteral("-version")});
    if (!versionProcess.waitForFinished(5000)) {
        return result;
    }
    result.version = parseVersion(versionProcess.readAllStandardOutput());

    QProcess encoderProcess;
    encoderProcess.start(result.ffmpegPath, {QStringLiteral("-encoders"), QStringLiteral("-hide_banner")});
    if (!encoderProcess.waitForFinished(5000)) {
        return result;
    }
    result.encoders = parseListOutput(encoderProcess.readAllStandardOutput());

    QProcess muxerProcess;
    muxerProcess.start(result.ffmpegPath, {QStringLiteral("-muxers"), QStringLiteral("-hide_banner")});
    if (!muxerProcess.waitForFinished(5000)) {
        return result;
    }
    result.muxers = parseListOutput(muxerProcess.readAllStandardOutput());

    result.success = true;
    return result;
}

bool FfmpegCapabilities::probe() {
    ProbeResult res = executeProbe();
    ffmpegPath_ = std::move(res.ffmpegPath);
    version_ = std::move(res.version);
    encoders_ = std::move(res.encoders);
    muxers_ = std::move(res.muxers);
    probed_ = res.success;
    emit probeCompleted();
    return probed_;
}

void FfmpegCapabilities::probeAsync() {
    if (probed_ || probing_) {
        return;
    }
    probing_ = true;
    emit probingChanged();

    QPointer<FfmpegCapabilities> self(this);
    QThread* thread = QThread::create([self]() {
        ProbeResult res = executeProbe();
        if (QCoreApplication::instance() != nullptr) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [self, res = std::move(res)]() mutable {
                if (!self) {
                    return;
                }
                self->ffmpegPath_ = std::move(res.ffmpegPath);
                self->version_ = std::move(res.version);
                self->encoders_ = std::move(res.encoders);
                self->muxers_ = std::move(res.muxers);
                self->probed_ = res.success;
                self->probing_ = false;
                emit self->probingChanged();
                emit self->probeCompleted();
            });
        }
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString FfmpegCapabilities::ffmpegPath() const {
    return ffmpegPath_;
}

QString FfmpegCapabilities::version() const {
    return version_;
}

bool FfmpegCapabilities::isProbed() const noexcept {
    return probed_;
}

bool FfmpegCapabilities::isProbing() const noexcept {
    return probing_;
}

bool FfmpegCapabilities::isEncoderAvailable(const QString& encoderName) const {
    return encoders_.contains(encoderName);
}

bool FfmpegCapabilities::isMuxerAvailable(const QString& muxerName) const {
    return muxers_.contains(muxerName);
}

QString FfmpegCapabilities::encoderForCodec(const AudioCodec codec) const {
    switch (codec) {
    case AudioCodec::Pcm: return QStringLiteral("pcm_s16le");
    case AudioCodec::Flac: return QStringLiteral("flac");
    case AudioCodec::Mp3: return QStringLiteral("libmp3lame");
    case AudioCodec::Vorbis: return QStringLiteral("libvorbis");
    }
    return {};
}

QString FfmpegCapabilities::muxerForContainer(const AudioContainer container) const {
    switch (container) {
    case AudioContainer::Wav: return QStringLiteral("wav");
    case AudioContainer::Flac: return QStringLiteral("flac");
    case AudioContainer::Ogg: return QStringLiteral("ogg");
    case AudioContainer::Mp3: return QStringLiteral("mp3");
    }
    return {};
}

bool FfmpegCapabilities::isTargetAvailable(const AudioContainer container,
                                            const AudioCodec codec) const {
    if (!probed_) {
        return true;
    }
    return isEncoderAvailable(encoderForCodec(codec)) &&
           isMuxerAvailable(muxerForContainer(container));
}

} // namespace abracapture::exporting
