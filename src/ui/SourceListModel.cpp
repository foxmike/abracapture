#include "SourceListModel.hpp"

#include <QMutexLocker>

SourceListModel::SourceListModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int SourceListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    QMutexLocker locker(&mutex_);
    return static_cast<int>(sources_.size());
}

QVariant SourceListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    QMutexLocker locker(&mutex_);
    const int row = index.row();
    if (row < 0 || row >= sources_.size()) {
        return {};
    }

    const SourceItem& item = sources_.at(row);

    switch (role) {
    case SourceIdRole:
    case NodeIdRole:
        return item.id;
    case DisplayNameRole:
    case Qt::DisplayRole:
        if (!item.displayName.isEmpty()) {
            return item.displayName;
        }
        if (!item.appName.isEmpty()) {
            return item.appName;
        }
        return QStringLiteral("Source %1").arg(item.id);
    case SourceKindRole:
        return item.sourceKind;
    case ChannelCountRole:
        return item.channelCount;
    case SampleRateRole:
        return item.sampleRate;
    case BitDepthRole:
        return item.bitDepth;
    case IsActiveRole:
        return item.isActive;
    case AppNameRole:
        return item.appName;
    case MediaClassRole:
        return item.mediaClass;
    case NodeNameRole:
        return item.nodeName;
    case IsSelectedRole:
        return (selectedSourceId_ != 0 && item.id == selectedSourceId_) ||
               (selectedIndex_ >= 0 && row == selectedIndex_);
    default:
        return {};
    }
}

QHash<int, QByteArray> SourceListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[SourceIdRole] = "sourceId";
    roles[DisplayNameRole] = "displayName";
    roles[SourceKindRole] = "sourceKind";
    roles[ChannelCountRole] = "channelCount";
    roles[SampleRateRole] = "sampleRate";
    roles[BitDepthRole] = "bitDepth";
    roles[IsActiveRole] = "isActive";
    roles[NodeIdRole] = "nodeId";
    roles[AppNameRole] = "appName";
    roles[MediaClassRole] = "mediaClass";
    roles[NodeNameRole] = "nodeName";
    roles[IsSelectedRole] = "isSelected";
    return roles;
}

int SourceListModel::selectedIndex() const {
    QMutexLocker locker(&mutex_);
    return selectedIndex_;
}

std::uint32_t SourceListModel::selectedSourceId() const {
    QMutexLocker locker(&mutex_);
    return selectedSourceId_;
}

bool SourceListModel::hasSelection() const {
    QMutexLocker locker(&mutex_);
    return selectedIndex_ >= 0 || selectedSourceId_ != 0;
}

void SourceListModel::refresh() {
    emit refreshRequested();
}

void SourceListModel::selectSource(int index) {
    std::uint32_t newSourceId = 0;
    int oldIndex = -1;
    {
        QMutexLocker locker(&mutex_);
        if (selectedIndex_ == index) {
            return;
        }
        oldIndex = selectedIndex_;
        selectedIndex_ = index;
        if (index >= 0 && index < sources_.size()) {
            selectedSourceId_ = sources_.at(index).id;
            newSourceId = selectedSourceId_;
        } else {
            selectedSourceId_ = 0;
            selectedIndex_ = -1;
        }
    }

    emit selectionChanged();
    if (newSourceId != 0) {
        emit sourceSelected(newSourceId);
    }

    if (oldIndex >= 0 && oldIndex < rowCount()) {
        const QModelIndex oldIdx = this->index(oldIndex);
        emit dataChanged(oldIdx, oldIdx, {IsSelectedRole});
    }
    if (selectedIndex_ >= 0 && selectedIndex_ < rowCount()) {
        const QModelIndex newIdx = this->index(selectedIndex_);
        emit dataChanged(newIdx, newIdx, {IsSelectedRole});
    }
}

void SourceListModel::selectSourceById(std::uint32_t sourceId) {
    int newIndex = -1;
    int oldIndex = -1;
    {
        QMutexLocker locker(&mutex_);
        if (selectedSourceId_ == sourceId && sourceId != 0) {
            return;
        }
        oldIndex = selectedIndex_;
        selectedSourceId_ = sourceId;

        if (sourceId != 0) {
            for (int i = 0; i < sources_.size(); ++i) {
                if (sources_.at(i).id == sourceId) {
                    newIndex = i;
                    break;
                }
            }
        }
        selectedIndex_ = newIndex;
    }

    emit selectionChanged();
    if (sourceId != 0) {
        emit sourceSelected(sourceId);
    }

    if (oldIndex >= 0 && oldIndex < rowCount()) {
        const QModelIndex oldIdx = this->index(oldIndex);
        emit dataChanged(oldIdx, oldIdx, {IsSelectedRole});
    }
    if (newIndex >= 0 && newIndex < rowCount()) {
        const QModelIndex newIdx = this->index(newIndex);
        emit dataChanged(newIdx, newIdx, {IsSelectedRole});
    }
}

void SourceListModel::clearSelection() {
    int oldIndex = -1;
    {
        QMutexLocker locker(&mutex_);
        if (selectedIndex_ == -1 && selectedSourceId_ == 0) {
            return;
        }
        oldIndex = selectedIndex_;
        selectedIndex_ = -1;
        selectedSourceId_ = 0;
    }

    emit selectionChanged();
    if (oldIndex >= 0 && oldIndex < rowCount()) {
        const QModelIndex oldIdx = this->index(oldIndex);
        emit dataChanged(oldIdx, oldIdx, {IsSelectedRole});
    }
}

QVariantMap SourceListModel::get(int index) const {
    QMutexLocker locker(&mutex_);
    if (index < 0 || index >= sources_.size()) {
        return {};
    }
    const SourceItem& item = sources_.at(index);
    QVariantMap map;
    map[QStringLiteral("sourceId")] = item.id;
    map[QStringLiteral("nodeId")] = item.id;
    map[QStringLiteral("displayName")] = item.displayName;
    map[QStringLiteral("appName")] = item.appName;
    map[QStringLiteral("sourceKind")] = item.sourceKind;
    map[QStringLiteral("channelCount")] = item.channelCount;
    map[QStringLiteral("sampleRate")] = item.sampleRate;
    map[QStringLiteral("bitDepth")] = item.bitDepth;
    map[QStringLiteral("isActive")] = item.isActive;
    map[QStringLiteral("mediaClass")] = item.mediaClass;
    map[QStringLiteral("nodeName")] = item.nodeName;
    map[QStringLiteral("isSelected")] = (selectedSourceId_ != 0 && item.id == selectedSourceId_) ||
                                         (selectedIndex_ >= 0 && index == selectedIndex_);
    return map;
}

QVariantMap SourceListModel::getSourceById(std::uint32_t sourceId) const {
    QMutexLocker locker(&mutex_);
    for (int i = 0; i < sources_.size(); ++i) {
        if (sources_.at(i).id == sourceId) {
            const SourceItem& item = sources_.at(i);
            QVariantMap map;
            map[QStringLiteral("sourceId")] = item.id;
            map[QStringLiteral("nodeId")] = item.id;
            map[QStringLiteral("displayName")] = item.displayName;
            map[QStringLiteral("appName")] = item.appName;
            map[QStringLiteral("sourceKind")] = item.sourceKind;
            map[QStringLiteral("channelCount")] = item.channelCount;
            map[QStringLiteral("sampleRate")] = item.sampleRate;
            map[QStringLiteral("bitDepth")] = item.bitDepth;
            map[QStringLiteral("isActive")] = item.isActive;
            map[QStringLiteral("mediaClass")] = item.mediaClass;
            map[QStringLiteral("nodeName")] = item.nodeName;
            map[QStringLiteral("isSelected")] = (selectedSourceId_ != 0 && item.id == selectedSourceId_) ||
                                                 (selectedIndex_ >= 0 && i == selectedIndex_);
            return map;
        }
    }
    return {};
}

int SourceListModel::indexOf(std::uint32_t sourceId) const {
    QMutexLocker locker(&mutex_);
    for (int i = 0; i < sources_.size(); ++i) {
        if (sources_.at(i).id == sourceId) {
            return i;
        }
    }
    return -1;
}

QString SourceListModel::sourceKindDisplay(const QString& category) const {
    const QString lower = category.toLower();
    if (lower.contains(QStringLiteral("application")) || lower.contains(QStringLiteral("playback"))) {
        return QStringLiteral("Application");
    }
    if (lower.contains(QStringLiteral("monitor")) || lower.contains(QStringLiteral("output"))) {
        return QStringLiteral("Monitor");
    }
    if (lower.contains(QStringLiteral("hardware")) || lower.contains(QStringLiteral("input")) || lower.contains(QStringLiteral("source"))) {
        return QStringLiteral("Hardware");
    }
    return QStringLiteral("Audio Source");
}

void SourceListModel::updateSources(const QList<SourceItem>& sources) {
    const int oldCount = rowCount();
    bool selectionRelocated = false;

    beginResetModel();
    {
        QMutexLocker locker(&mutex_);
        sources_ = sources;

        // Re-locate selected index by selectedSourceId if set
        if (selectedSourceId_ != 0) {
            int newIdx = -1;
            for (int i = 0; i < sources_.size(); ++i) {
                if (sources_.at(i).id == selectedSourceId_) {
                    newIdx = i;
                    break;
                }
            }
            if (newIdx != selectedIndex_) {
                selectedIndex_ = newIdx;
                selectionRelocated = true;
            }
        } else if (selectedIndex_ >= sources_.size()) {
            selectedIndex_ = -1;
            selectionRelocated = true;
        }
    }
    endResetModel();

    if (oldCount != rowCount()) {
        emit countChanged();
    }
    if (selectionRelocated) {
        emit selectionChanged();
    }
}

void SourceListModel::updateSourcesFromVariants(const QVariantList& sources) {
    QList<SourceItem> items;
    items.reserve(sources.size());

    for (const QVariant& var : sources) {
        const QVariantMap map = var.toMap();
        SourceItem item;
        item.id = map.value(QStringLiteral("sourceId"), map.value(QStringLiteral("nodeId"), 0)).toUInt();
        item.displayName = map.value(QStringLiteral("displayName")).toString();
        item.appName = map.value(QStringLiteral("appName")).toString();
        item.sourceKind = map.value(QStringLiteral("sourceKind")).toString();
        item.channelCount = map.value(QStringLiteral("channelCount"), 2).toInt();
        item.sampleRate = map.value(QStringLiteral("sampleRate"), 48000).toInt();
        item.bitDepth = map.value(QStringLiteral("bitDepth"), 0).toInt();
        item.isActive = map.value(QStringLiteral("isActive"), false).toBool();
        item.mediaClass = map.value(QStringLiteral("mediaClass")).toString();
        item.nodeName = map.value(QStringLiteral("nodeName")).toString();
        items.append(item);
    }

    updateSources(items);
}

void SourceListModel::addOrUpdateSource(const SourceItem& source) {
    int existingIndex = -1;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_.at(i).id == source.id) {
                existingIndex = i;
                break;
            }
        }
    }

    if (existingIndex >= 0) {
        {
            QMutexLocker locker(&mutex_);
            sources_[existingIndex] = source;
        }
        const QModelIndex idx = index(existingIndex);
        emit dataChanged(idx, idx);
    } else {
        const int newRow = rowCount();
        beginInsertRows(QModelIndex(), newRow, newRow);
        {
            QMutexLocker locker(&mutex_);
            sources_.append(source);
            if (selectedSourceId_ != 0 && source.id == selectedSourceId_) {
                selectedIndex_ = newRow;
            }
        }
        endInsertRows();
        emit countChanged();
    }
}

void SourceListModel::removeSource(std::uint32_t sourceId) {
    int targetIndex = -1;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_.at(i).id == sourceId) {
                targetIndex = i;
                break;
            }
        }
    }

    if (targetIndex >= 0) {
        beginRemoveRows(QModelIndex(), targetIndex, targetIndex);
        bool selectionCleared = false;
        {
            QMutexLocker locker(&mutex_);
            sources_.removeAt(targetIndex);
            if (selectedSourceId_ == sourceId) {
                selectedSourceId_ = 0;
                selectedIndex_ = -1;
                selectionCleared = true;
            } else if (selectedIndex_ > targetIndex) {
                selectedIndex_--;
                selectionCleared = true;
            }
        }
        endRemoveRows();
        emit countChanged();
        if (selectionCleared) {
            emit selectionChanged();
        }
    }
}

void SourceListModel::setActiveSourceId(std::uint32_t activeId) {
    QList<int> changedRows;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            const bool shouldBeActive = (sources_.at(i).id == activeId);
            if (sources_.at(i).isActive != shouldBeActive) {
                sources_[i].isActive = shouldBeActive;
                changedRows.append(i);
            }
        }
    }
    for (const int row : changedRows) {
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {IsActiveRole});
    }
}

void SourceListModel::setSourceActive(std::uint32_t sourceId, bool active) {
    int changedRow = -1;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_.at(i).id == sourceId) {
                if (sources_.at(i).isActive != active) {
                    sources_[i].isActive = active;
                    changedRow = i;
                }
                break;
            }
        }
    }
    if (changedRow >= 0) {
        const QModelIndex idx = index(changedRow);
        emit dataChanged(idx, idx, {IsActiveRole});
    }
}

void SourceListModel::updateNegotiatedFormat(const std::uint32_t sourceId,
                                             const int sampleRate,
                                             const int bitDepth,
                                             const int channels) {
    int changedRow = -1;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < static_cast<int>(sources_.size()); ++i) {
            if (sources_.at(i).id != sourceId) continue;
            SourceItem& item = sources_[i];
            const bool changed = item.sampleRate != sampleRate || item.bitDepth != bitDepth ||
                                 (channels > 0 && item.channelCount != channels);
            if (!changed) return;
            item.sampleRate = sampleRate;
            item.bitDepth = bitDepth;
            if (channels > 0) item.channelCount = channels;
            changedRow = i;
            break;
        }
    }
    if (changedRow >= 0) {
        const QModelIndex changedIndex = index(changedRow);
        emit dataChanged(changedIndex, changedIndex, {SampleRateRole, BitDepthRole, ChannelCountRole});
    }
}

void SourceListModel::clear() {
    const int oldCount = rowCount();
    beginResetModel();
    {
        QMutexLocker locker(&mutex_);
        sources_.clear();
        selectedIndex_ = -1;
        selectedSourceId_ = 0;
    }
    endResetModel();

    if (oldCount > 0) {
        emit countChanged();
        emit selectionChanged();
    }
}
