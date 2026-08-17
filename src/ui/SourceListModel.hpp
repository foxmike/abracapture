#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <cstdint>

struct SourceItem {
    std::uint32_t id = 0;
    QString displayName;
    QString appName;
    QString sourceKind; // e.g. "Application", "Monitor", "Hardware", "Other"
    int channelCount = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    QString sampleFormat;
    bool isActive = false;
    QString mediaClass;
    QString nodeName;

    bool operator==(const SourceItem& other) const noexcept {
        return id == other.id &&
               displayName == other.displayName &&
               appName == other.appName &&
               sourceKind == other.sourceKind &&
               channelCount == other.channelCount &&
               sampleRate == other.sampleRate &&
               bitDepth == other.bitDepth &&
               sampleFormat == other.sampleFormat &&
               isActive == other.isActive &&
               mediaClass == other.mediaClass &&
               nodeName == other.nodeName;
    }
};

class SourceListModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE selectSource NOTIFY selectionChanged)
    Q_PROPERTY(std::uint32_t selectedSourceId READ selectedSourceId WRITE selectSourceById NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

public:
    enum SourceRoles {
        SourceIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        SourceKindRole,
        ChannelCountRole,
        SampleRateRole,
        BitDepthRole,
        SampleFormatRole,
        IsActiveRole,
        NodeIdRole,
        AppNameRole,
        MediaClassRole,
        NodeNameRole,
        IsSelectedRole
    };
    Q_ENUM(SourceRoles)

    explicit SourceListModel(QObject* parent = nullptr);
    ~SourceListModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedIndex() const;
    std::uint32_t selectedSourceId() const;
    bool hasSelection() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectSource(int index);
    Q_INVOKABLE void selectSourceById(std::uint32_t sourceId);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QVariantMap getSourceById(std::uint32_t sourceId) const;
    Q_INVOKABLE int indexOf(std::uint32_t sourceId) const;
    Q_INVOKABLE QString sourceKindDisplay(const QString& category) const;

public slots:
    void updateSources(const QList<SourceItem>& sources);
    void updateSourcesFromVariants(const QVariantList& sources);
    void addOrUpdateSource(const SourceItem& source);
    void removeSource(std::uint32_t sourceId);
    void setActiveSourceId(std::uint32_t activeId);
    void setSourceActive(std::uint32_t sourceId, bool active);
    void updateNegotiatedFormat(std::uint32_t sourceId, int sampleRate, int bitDepth, int channels);
    void clear();

signals:
    void countChanged();
    void selectionChanged();
    void refreshRequested();
    void sourceSelected(std::uint32_t sourceId);

private:
    mutable QMutex mutex_;
    QList<SourceItem> sources_;
    int selectedIndex_ = -1;
    std::uint32_t selectedSourceId_ = 0;
};
