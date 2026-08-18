#pragma once

#include "history/RecordingIndex.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class RecordingsViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedEntry READ selectedEntry NOTIFY selectedIndexChanged)
    Q_PROPERTY(int exportSelectionCount READ exportSelectionCount NOTIFY exportSelectionChanged)
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
        SidecarPathRole,
        CaptureSessionIdRole,
        DerivedExportsRole,
        TrackTitleRole,
        AlbumTitleRole,
        ApplicationNameRole,
        MediaNameRole
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
    Q_INVOKABLE bool isSelectedForExport(int index) const;
    Q_INVOKABLE void toggleExportSelection(int index);
    Q_INVOKABLE void clearExportSelection();
    Q_INVOKABLE QVariantList exportSelectedEntries() const;
    Q_INVOKABLE bool removeEntry(int index);
    Q_INVOKABLE void refreshEntries();
    Q_INVOKABLE bool importDirectory(const QString& directoryPath);
    Q_INVOKABLE QVariantMap get(int index) const;

signals:
    void countChanged();
    void selectedIndexChanged();
    void operationStatusChanged();
    void exportSelectionChanged();

private:
    void syncEntries();
    void rebuildModel();
    QString operationStatus() const;
    [[nodiscard]] int exportSelectionCount() const noexcept;

    abracapture::history::RecordingIndex* externalIndex_ = nullptr;
    abracapture::history::RecordingIndex ownedIndex_;
    QList<abracapture::history::RecordingEntry> entries_;
    int selectedIndex_ = -1;
    QString operationStatus_;
    QString recordingsDirectory_;
    QSet<QString> exportSelectedPaths_;
};
