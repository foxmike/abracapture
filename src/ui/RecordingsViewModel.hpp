#pragma once

#include "history/RecordingIndex.hpp"
#include "export/ExportManager.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class RecordingsViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedEntry READ selectedEntry NOTIFY selectedIndexChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY operationStatusChanged)

public:
    enum RecordingRoles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        FormatRole,
        SampleRateRole,
        BitDepthRole,
        ChannelsRole,
        DurationRole,
        TimestampRole,
        IsNativeRole,
        SidecarPathRole
    };
    Q_ENUM(RecordingRoles)

    explicit RecordingsViewModel(abracapture::history::RecordingIndex* index = nullptr,
                                 QObject* parent = nullptr);
    explicit RecordingsViewModel(QObject* parent);
    ~RecordingsViewModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] int selectedIndex() const noexcept;
    void setSelectedIndex(int index);
    [[nodiscard]] QVariantMap selectedEntry() const;

    [[nodiscard]] abracapture::history::RecordingIndex* recordingIndex() const noexcept;
    void setRecordingIndex(abracapture::history::RecordingIndex* index);

    Q_INVOKABLE bool revealInFileManager(int index);
    Q_INVOKABLE bool playEntry(int index);
    Q_INVOKABLE bool convertEntry(int index, const QString& format);
    Q_INVOKABLE bool removeEntry(int index);
    Q_INVOKABLE void refreshEntries();
    Q_INVOKABLE bool importDirectory(const QString& directoryPath);
    Q_INVOKABLE QVariantMap get(int index) const;

signals:
    void countChanged();
    void selectedIndexChanged();
    void operationStatusChanged();

private:
    void syncEntries();
    QString operationStatus() const;

    abracapture::history::RecordingIndex* externalIndex_ = nullptr;
    abracapture::history::RecordingIndex ownedIndex_;
    QList<abracapture::history::RecordingEntry> entries_;
    int selectedIndex_ = -1;
    QString operationStatus_;
    abracapture::exporting::ExportManager exportManager_;
};
