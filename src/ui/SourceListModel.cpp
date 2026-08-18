#include "SourceListModel.hpp"

#include <QMutexLocker>

#include <cstdio>

namespace {

bool sameReplacementGroup(const SourceItem& left, const SourceItem& right)
{
    if (!left.logicalIdentity.isEmpty() && !right.logicalIdentity.isEmpty() &&
        left.logicalIdentity == right.logicalIdentity) {
        return true;
    }
    return !left.mediaClass.isEmpty() && left.mediaClass == right.mediaClass &&
           !left.clientId.isEmpty() && left.clientId == right.clientId &&
           !left.appName.isEmpty() && left.appName == right.appName &&
           !left.nodeName.isEmpty() && left.nodeName == right.nodeName;
}

} // namespace

SourceListModel::SourceListModel(QObject* parent)
    : QAbstractListModel(parent) {
    selectionExpiryTimer_ = new QTimer(this);
    selectionExpiryTimer_->setSingleShot(true);
    // Firefox may announce the replacement node before its media metadata;
    // allow that bounded interval for the candidate to mature.
    selectionExpiryTimer_->setInterval(3000);
    connect(selectionExpiryTimer_, &QTimer::timeout, this,
            &SourceListModel::resolvePendingReplacement);
}

void SourceListModel::resolvePendingReplacement()
{
    SourceItem replacement;
    std::uint32_t oldNodeId = 0;
    QString oldSerial;
    int replacementRow = -1;
    bool resolved = false;
    bool cleared = false;
    bool ambiguous = false;
    int candidateCount = 0;
    int rawCandidateCount = 0;

    {
        QMutexLocker locker(&mutex_);
        if (!replacementPending_ || selectedSourceId_ == 0 || selectedIndex_ >= 0) {
            return;
        }

        QList<int> candidates;
        const auto formatCompatible = [this](const SourceItem& item) {
            const SourceItem& old = pendingReplacementSource_;
            return (old.sampleRate <= 0 || item.sampleRate <= 0 ||
                    old.sampleRate == item.sampleRate) &&
                   (old.channelCount <= 0 || item.channelCount <= 0 ||
                    old.channelCount == item.channelCount) &&
                   (old.sampleFormat.isEmpty() || item.sampleFormat.isEmpty() ||
                    old.sampleFormat == QStringLiteral("Unknown") ||
                    item.sampleFormat == QStringLiteral("Unknown") ||
                    old.sampleFormat == item.sampleFormat);
        };
        for (const std::uint32_t candidateId : replacementCandidateIds_) {
            for (int row = 0; row < sources_.size(); ++row) {
                const SourceItem& item = sources_.at(row);
                if (item.id == candidateId &&
                    sameReplacementGroup(item, pendingReplacementSource_)) {
                    ++rawCandidateCount;
                    if (formatCompatible(item)) {
                        candidates.append(row);
                    }
                    break;
                }
            }
        }
        candidateCount = candidates.size();
        if (candidateCount == 1) {
            replacementRow = candidates.front();
            replacement = sources_.at(replacementRow);
            oldNodeId = pendingReplacementSource_.id;
            oldSerial = pendingReplacementSource_.objectSerial;
            selectedSourceId_ = replacement.id;
            selectedObjectSerial_ = replacement.objectSerial;
            selectedLogicalIdentity_ = replacement.logicalIdentity;
            selectedInstanceIdentity_ = replacement.instanceIdentity;
            selectedIndex_ = replacementRow;
            replacementPending_ = false;
            replacementCandidateIds_.clear();
            replacementMaturedCandidateIds_.clear();
            resolved = true;
        } else if (candidateCount > 1 || rawCandidateCount > 0) {
            // Do not attach to an arbitrary sibling stream. Preserve the
            // detached logical selection and let a later explicit selection
            // or catalog change resolve it.
            ambiguous = true;
            if (selectionExpiryTimer_ != nullptr) {
                selectionExpiryTimer_->stop();
            }
        } else {
            selectedSourceId_ = 0;
            selectedObjectSerial_.clear();
            selectedLogicalIdentity_.clear();
            selectedInstanceIdentity_.clear();
            selectedIndex_ = -1;
            replacementPending_ = false;
            replacementCandidateIds_.clear();
            replacementMaturedCandidateIds_.clear();
            cleared = true;
        }
    }

    if (resolved) {
        std::fprintf(stderr,
                     "[abracapture] cross-node replacement resolved: old node=%u serial=%s "
                     "new node=%u serial=%s\n",
                     oldNodeId,
                     oldSerial.toUtf8().constData(),
                     replacement.id,
                     replacement.objectSerial.toUtf8().constData());
        std::fprintf(stderr,
                     "[abracapture] source event=replacement old.node.id=%u old.serial=%s "
                     "node.id=%u new.serial=%s selection=reconciled\n",
                     oldNodeId,
                     oldSerial.toUtf8().constData(),
                     replacement.id,
                     replacement.objectSerial.toUtf8().constData());
        emit selectionChanged();
        emit sourceSelected(replacement.id);
        emit sourceInstanceChanged(replacement.id, replacement.objectSerial);
        if (replacementRow >= 0) {
            const QModelIndex index = this->index(replacementRow);
            emit dataChanged(index, index, {IsSelectedRole});
        }
    } else if (ambiguous) {
            std::fprintf(stderr,
                         "[abracapture] cross-node replacement ambiguous candidates=%d; "
                         "selection retained pending a unique candidate\n",
                         rawCandidateCount);
    } else if (cleared) {
        emit selectionChanged();
    }
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
    case SampleFormatRole:
        return item.sampleFormat;
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
    roles[SampleFormatRole] = "sampleFormat";
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
        // An explicit user selection supersedes any pending replacement.
        replacementPending_ = false;
        replacementCandidateIds_.clear();
        replacementMaturedCandidateIds_.clear();
        if (selectionExpiryTimer_ != nullptr) {
            selectionExpiryTimer_->stop();
        }
        if (selectedIndex_ == index) {
            return;
        }
        oldIndex = selectedIndex_;
        selectedIndex_ = index;
        if (index >= 0 && index < sources_.size()) {
            selectedSourceId_ = sources_.at(index).id;
            selectedObjectSerial_ = sources_.at(index).objectSerial;
            selectedLogicalIdentity_ = sources_.at(index).logicalIdentity;
            selectedInstanceIdentity_ = sources_.at(index).instanceIdentity;
            newSourceId = selectedSourceId_;
        } else {
            selectedSourceId_ = 0;
            selectedObjectSerial_.clear();
            selectedLogicalIdentity_.clear();
            selectedInstanceIdentity_.clear();
            selectedIndex_ = -1;
        }
    }

    emit selectionChanged();
    if (newSourceId != 0) {
        emit sourceSelected(newSourceId);
        emit sourceInstanceChanged(newSourceId, selectedObjectSerial_);
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
        // An explicit user selection supersedes any pending replacement.
        replacementPending_ = false;
        replacementCandidateIds_.clear();
        replacementMaturedCandidateIds_.clear();
        if (selectionExpiryTimer_ != nullptr) {
            selectionExpiryTimer_->stop();
        }
        oldIndex = selectedIndex_;

        if (sourceId != 0) {
            for (int i = 0; i < sources_.size(); ++i) {
                if (sources_.at(i).id == sourceId) {
                    newIndex = i;
                    break;
                }
            }
        }
        if (selectedSourceId_ == sourceId &&
            ((sourceId == 0 && selectedIndex_ < 0) ||
             (sourceId != 0 && selectedIndex_ == newIndex && newIndex >= 0))) {
            return;
        }
        selectedSourceId_ = sourceId;
        selectedIndex_ = newIndex;
        if (newIndex >= 0) {
            selectedObjectSerial_ = sources_.at(newIndex).objectSerial;
            selectedLogicalIdentity_ = sources_.at(newIndex).logicalIdentity;
            selectedInstanceIdentity_ = sources_.at(newIndex).instanceIdentity;
        } else if (sourceId == 0) {
            selectedObjectSerial_.clear();
            selectedLogicalIdentity_.clear();
            selectedInstanceIdentity_.clear();
        }
    }

    emit selectionChanged();
    if (sourceId != 0) {
        emit sourceSelected(sourceId);
        emit sourceInstanceChanged(sourceId, selectedObjectSerial_);
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
        selectedObjectSerial_.clear();
        selectedLogicalIdentity_.clear();
        selectedInstanceIdentity_.clear();
        replacementPending_ = false;
        replacementCandidateIds_.clear();
        replacementMaturedCandidateIds_.clear();
        if (selectionExpiryTimer_ != nullptr) {
            selectionExpiryTimer_->stop();
        }
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
    map[QStringLiteral("sampleFormat")] = item.sampleFormat;
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
            map[QStringLiteral("sampleFormat")] = item.sampleFormat;
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
    bool instanceChanged = false;
    QString instanceSerial;

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
                    instanceChanged = selectedObjectSerial_ != sources_.at(i).objectSerial;
                    selectedObjectSerial_ = sources_.at(i).objectSerial;
                    selectedLogicalIdentity_ = sources_.at(i).logicalIdentity;
                    selectedInstanceIdentity_ = sources_.at(i).instanceIdentity;
                    break;
                }
            }
            // A catalog refresh can deliver a replacement row in the same
            // reset. Prefer the per-stream identity, then use the stable group
            // only when it has a single candidate; never guess among multiple
            // simultaneous streams from the same application.
            if (newIdx < 0 && !selectedInstanceIdentity_.isEmpty()) {
                for (int i = 0; i < sources_.size(); ++i) {
                    if (sources_.at(i).instanceIdentity == selectedInstanceIdentity_) {
                        newIdx = i;
                        selectedSourceId_ = sources_.at(i).id;
                        instanceChanged = true;
                        selectedObjectSerial_ = sources_.at(i).objectSerial;
                        selectedLogicalIdentity_ = sources_.at(i).logicalIdentity;
                        break;
                    }
                }
            }
            if (newIdx < 0 && !selectedLogicalIdentity_.isEmpty()) {
                int groupMatches = 0;
                int groupIndex = -1;
                for (int i = 0; i < sources_.size(); ++i) {
                    if (sources_.at(i).logicalIdentity == selectedLogicalIdentity_) {
                        ++groupMatches;
                        groupIndex = i;
                    }
                }
                if (groupMatches == 1) {
                    newIdx = groupIndex;
                    selectedSourceId_ = sources_.at(groupIndex).id;
                    instanceChanged = true;
                    selectedObjectSerial_ = sources_.at(groupIndex).objectSerial;
                    selectedInstanceIdentity_ = sources_.at(groupIndex).instanceIdentity;
                }
            }
            if (newIdx != selectedIndex_) {
                selectedIndex_ = newIdx;
                selectionRelocated = true;
            }
            if (newIdx < 0 && selectionExpiryTimer_ != nullptr) {
                selectionExpiryTimer_->start();
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
    if (instanceChanged && selectedSourceId_ != 0) {
        instanceSerial = selectedObjectSerial_;
        emit sourceInstanceChanged(selectedSourceId_, instanceSerial);
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
        item.channelCount = map.value(QStringLiteral("channelCount"), 0).toInt();
        item.sampleRate = map.value(QStringLiteral("sampleRate"), 0).toInt();
        item.bitDepth = map.value(QStringLiteral("bitDepth"), 0).toInt();
        item.sampleFormat = map.value(QStringLiteral("sampleFormat")).toString();
        item.isActive = map.value(QStringLiteral("isActive"), false).toBool();
        item.mediaClass = map.value(QStringLiteral("mediaClass")).toString();
        item.nodeName = map.value(QStringLiteral("nodeName")).toString();
        item.clientId = map.value(QStringLiteral("clientId")).toString();
        item.mediaName = map.value(QStringLiteral("mediaName")).toString();
        item.mediaTitle = map.value(QStringLiteral("mediaTitle")).toString();
        item.mediaAlbum = map.value(QStringLiteral("mediaAlbum")).toString();
        item.objectSerial = map.value(QStringLiteral("objectSerial")).toString();
        item.logicalIdentity = map.value(QStringLiteral("logicalIdentity")).toString();
        item.instanceIdentity = map.value(QStringLiteral("instanceIdentity")).toString();
        if (item.logicalIdentity.isEmpty()) {
            item.logicalIdentity = item.mediaClass + QChar(0x1f) + item.appName +
                                   QChar(0x1f) + item.nodeName;
        }
        if (item.instanceIdentity.isEmpty()) {
            item.instanceIdentity = item.logicalIdentity;
        }
        items.append(item);
    }

    updateSources(items);
}

void SourceListModel::addOrUpdateSource(const SourceItem& source) {
    int existingIndex = -1;
    bool instanceMatch = false;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_.at(i).id == source.id) {
                existingIndex = i;
                break;
            }
            if (!source.instanceIdentity.isEmpty() &&
                sources_.at(i).instanceIdentity == source.instanceIdentity) {
                existingIndex = i;
                instanceMatch = true;
                break;
            }
        }
    }

    if (existingIndex >= 0) {
        std::uint32_t oldId = 0;
        QString oldSerial;
        QString oldLogicalIdentity;
        QString oldMediaName;
        bool selectionRelocated = false;
        bool selectionWasDetached = false;
        bool selectedInstanceChanged = false;
        bool replacementCandidateAdded = false;
        bool replacementCandidateMatured = false;
        {
            QMutexLocker locker(&mutex_);
            oldId = sources_.at(existingIndex).id;
            oldSerial = sources_.at(existingIndex).objectSerial;
            oldLogicalIdentity = sources_.at(existingIndex).logicalIdentity;
            oldMediaName = sources_.at(existingIndex).mediaName;
            selectionWasDetached = selectedSourceId_ != 0 && selectedIndex_ < 0;
            const bool wasPendingCandidate = replacementCandidateIds_.contains(source.id);
            const bool compatiblePendingCandidate = replacementPending_ &&
                selectedIndex_ < 0 && source.id != pendingReplacementSource_.id &&
                sameReplacementGroup(source, pendingReplacementSource_);
            sources_[existingIndex] = source;
            if (compatiblePendingCandidate && !wasPendingCandidate) {
                replacementCandidateIds_.insert(source.id);
                replacementCandidateAdded = true;
            } else if (compatiblePendingCandidate && wasPendingCandidate &&
                       !replacementMaturedCandidateIds_.contains(source.id) &&
                       oldMediaName.isEmpty() && !source.mediaName.isEmpty()) {
                replacementMaturedCandidateIds_.insert(source.id);
                replacementCandidateMatured = true;
            }
            const bool selectedRow = selectedSourceId_ == source.id ||
                (!selectedInstanceIdentity_.isEmpty() &&
                 selectedInstanceIdentity_ == source.instanceIdentity);
            if (selectedRow) {
                selectedInstanceChanged = selectedObjectSerial_ != source.objectSerial;
                selectionRelocated = selectedIndex_ != existingIndex ||
                                     selectedSourceId_ != source.id;
                selectedSourceId_ = source.id;
                selectedObjectSerial_ = source.objectSerial;
                selectedIndex_ = existingIndex;
                selectedLogicalIdentity_ = source.logicalIdentity;
                selectedInstanceIdentity_ = source.instanceIdentity;
                replacementPending_ = false;
                replacementCandidateIds_.clear();
                replacementMaturedCandidateIds_.clear();
                if (selectionExpiryTimer_ != nullptr) {
                    selectionExpiryTimer_->stop();
                }
            }
        }
        const QModelIndex idx = index(existingIndex);
        emit dataChanged(idx, idx);
        if (replacementCandidateAdded) {
            if (selectionExpiryTimer_ != nullptr) {
                selectionExpiryTimer_->start();
            }
            std::fprintf(stderr,
                         "[abracapture] replacement candidate added node=%u serial=%s "
                         "preexisting=false\n",
                         source.id,
                         source.objectSerial.toUtf8().constData());
        }
        if (replacementCandidateMatured) {
            std::fprintf(stderr,
                         "[abracapture] replacement candidate metadata matured node=%u "
                         "media.name=%s\n",
                         source.id,
                         source.mediaName.toUtf8().constData());
        }
        if (instanceMatch && oldId != source.id && selectionRelocated) {
            std::fprintf(stderr,
                         "[abracapture] source event=replacement old.node.id=%u old.serial=%s "
                         "node.id=%u new.serial=%s logical-key=%s selection=reconciled\n",
                         oldId,
                         oldSerial.toUtf8().constData(),
                         source.id,
                         source.objectSerial.toUtf8().constData(),
                         source.logicalIdentity.toUtf8().constData());
            emit selectionChanged();
            emit sourceSelected(source.id);
            emit sourceInstanceChanged(source.id, source.objectSerial);
        } else if (selectionWasDetached && selectionRelocated) {
            std::fprintf(stderr,
                         "[abracapture] source event=replacement old.node.id=%u old.serial=%s "
                         "node.id=%u new.serial=%s logical-key=%s selection=reconciled\n",
                         oldId,
                         oldSerial.toUtf8().constData(),
                         source.id,
                         source.objectSerial.toUtf8().constData(),
                         source.logicalIdentity.toUtf8().constData());
            emit selectionChanged();
            emit sourceSelected(source.id);
            emit sourceInstanceChanged(source.id, source.objectSerial);
        } else if (selectedInstanceChanged) {
            std::fprintf(stderr,
                         "[abracapture] selected target instance changed node=%u old.serial=%s "
                         "new.serial=%s\n",
                         source.id,
                         oldSerial.toUtf8().constData(),
                         source.objectSerial.toUtf8().constData());
            emit sourceInstanceChanged(source.id, source.objectSerial);
        } else if (selectionRelocated) {
            emit selectionChanged();
        }
    } else {
        const int newRow = rowCount();
        bool selectionReconciled = false;
        bool ambiguousReplacement = false;
        int ambiguousCandidateCount = 0;
        bool selectedIdReused = false;
        bool replacementCandidateAdded = false;
        bool deferCrossNodeCandidate = false;
        QString oldSelectedSerial;
        beginInsertRows(QModelIndex(), newRow, newRow);
        {
            QMutexLocker locker(&mutex_);
            oldSelectedSerial = selectedObjectSerial_;
            deferCrossNodeCandidate = replacementPending_ && selectedIndex_ < 0 &&
                source.id != pendingReplacementSource_.id &&
                sameReplacementGroup(source, pendingReplacementSource_);
            sources_.append(source);
            if (deferCrossNodeCandidate) {
                replacementCandidateIds_.insert(source.id);
                replacementCandidateAdded = true;
            }
            selectedIdReused = selectedSourceId_ != 0 && selectedSourceId_ == source.id &&
                               (selectedLogicalIdentity_.isEmpty() ||
                                selectedLogicalIdentity_ == source.logicalIdentity);
            int groupMatches = 0;
            for (int i = 0; i < sources_.size() - 1; ++i) {
                if (sources_.at(i).logicalIdentity == source.logicalIdentity) {
                    ++groupMatches;
                }
            }
            const bool uniqueGroupReplacement = !selectedLogicalIdentity_.isEmpty() &&
                source.logicalIdentity == selectedLogicalIdentity_ && groupMatches == 0;
            const bool sameInstanceReplacement = !selectedInstanceIdentity_.isEmpty() &&
                source.instanceIdentity == selectedInstanceIdentity_;
            if (!deferCrossNodeCandidate &&
                (selectedIdReused || sameInstanceReplacement || uniqueGroupReplacement)) {
                selectedSourceId_ = source.id;
                selectedObjectSerial_ = source.objectSerial;
                selectedIndex_ = newRow;
                selectedLogicalIdentity_ = source.logicalIdentity;
                selectedInstanceIdentity_ = source.instanceIdentity;
                replacementPending_ = false;
                replacementCandidateIds_.clear();
                replacementMaturedCandidateIds_.clear();
                selectionReconciled = true;
                if (selectionExpiryTimer_ != nullptr) {
                    selectionExpiryTimer_->stop();
                }
            } else if (!deferCrossNodeCandidate && !selectedLogicalIdentity_.isEmpty() &&
                       source.logicalIdentity == selectedLogicalIdentity_ && groupMatches > 0) {
                ambiguousReplacement = true;
                ambiguousCandidateCount = groupMatches + 1;
            }
        }
        endInsertRows();
        emit countChanged();
        if (replacementCandidateAdded) {
            if (selectionExpiryTimer_ != nullptr) {
                selectionExpiryTimer_->start();
            }
            std::fprintf(stderr,
                         "[abracapture] replacement candidate added node=%u serial=%s "
                         "preexisting=false\n",
                         source.id,
                         source.objectSerial.toUtf8().constData());
        }
        if (selectionReconciled) {
            std::fprintf(stderr,
                         "[abracapture] source event=replacement old.serial=%s node.id=%u "
                         "new.serial=%s logical-key=%s selection=reconciled\n",
                         oldSelectedSerial.toUtf8().constData(),
                         source.id,
                         source.objectSerial.toUtf8().constData(),
                         source.logicalIdentity.toUtf8().constData());
            emit selectionChanged();
            emit sourceSelected(source.id);
            emit sourceInstanceChanged(source.id, source.objectSerial);
        } else if (ambiguousReplacement) {
            std::fprintf(stderr,
                         "[abracapture] source replacement ambiguous node=%u logical-key=%s; "
                         "candidates=%d; selection retained pending a unique candidate\n",
                         source.id,
                         source.logicalIdentity.toUtf8().constData(),
                         ambiguousCandidateCount);
        }
    }
}

void SourceListModel::removeSource(std::uint32_t sourceId) {
    int targetIndex = -1;
    SourceItem removedSource;
    {
        QMutexLocker locker(&mutex_);
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_.at(i).id == sourceId) {
                targetIndex = i;
                removedSource = sources_.at(i);
                break;
            }
        }
    }

    if (targetIndex >= 0) {
        beginRemoveRows(QModelIndex(), targetIndex, targetIndex);
        bool selectionCleared = false;
        bool replacementCandidateRemoved = false;
        {
            QMutexLocker locker(&mutex_);
            sources_.removeAt(targetIndex);
            if (replacementPending_) {
                replacementCandidateRemoved = replacementCandidateIds_.remove(sourceId) > 0;
                replacementMaturedCandidateIds_.remove(sourceId);
            }
            if (selectedSourceId_ == sourceId) {
                pendingReplacementSource_ = removedSource;
                replacementPending_ = true;
                replacementCandidateIds_.clear();
                replacementMaturedCandidateIds_.clear();
                selectedObjectSerial_ = removedSource.objectSerial;
                selectedLogicalIdentity_ = removedSource.logicalIdentity;
                selectedInstanceIdentity_ = removedSource.instanceIdentity;
                selectedIndex_ = -1;
                std::fprintf(stderr,
                             "[abracapture] selected instance removed node=%u serial=%s; "
                             "replacement grace started\n",
                             removedSource.id,
                             removedSource.objectSerial.toUtf8().constData());
                if (selectionExpiryTimer_ != nullptr) {
                    selectionExpiryTimer_->start();
                }
            } else if (replacementCandidateRemoved && selectionExpiryTimer_ != nullptr) {
                // A previously ambiguous candidate disappeared; give the
                // remaining newly-created candidates another bounded chance.
                selectionExpiryTimer_->start();
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
        selectedObjectSerial_.clear();
        selectedLogicalIdentity_.clear();
        selectedInstanceIdentity_.clear();
        replacementPending_ = false;
        replacementCandidateIds_.clear();
        replacementMaturedCandidateIds_.clear();
        if (selectionExpiryTimer_ != nullptr) {
            selectionExpiryTimer_->stop();
        }
    }
    endResetModel();

    if (oldCount > 0) {
        emit countChanged();
        emit selectionChanged();
    }
}
