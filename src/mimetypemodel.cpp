#include "mimetypemodel.h"

#include <QDebug>
#include <QIcon>
#include <QMimeDatabase>
#include <QTextStream>
#include <QVariant>

#include <algorithm>

Q_DECLARE_METATYPE(QMimeType)

using StandardItemList = QList<QStandardItem *>;

enum { mimeTypeRole = Qt::UserRole + 1, iconQueriedRole = Qt::UserRole + 2 };

QT_BEGIN_NAMESPACE
bool operator<(const QMimeType &t1, const QMimeType &t2)
{
    return t1.name() < t2.name();
}
QT_END_NAMESPACE

static StandardItemList createRow(const QMimeType &t)
{
    const QVariant v = QVariant::fromValue(t);
    QStandardItem *nameItem = new QStandardItem(t.name());
    const Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    nameItem->setData(v, mimeTypeRole);
    nameItem->setData(QVariant(false), iconQueriedRole);
    nameItem->setFlags(flags);
    nameItem->setToolTip(t.comment());
    return StandardItemList{nameItem};
}

MimetypeModel::MimetypeModel(QObject *parent)
    : QStandardItemModel(0, ColumnCount, parent)
{
    setHorizontalHeaderLabels(QStringList{tr("Name")});
    populate();
}

QVariant MimetypeModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DecorationRole || !index.isValid() || index.data(iconQueriedRole).toBool())
        return QStandardItemModel::data(index, role);
    QStandardItem *item = itemFromIndex(index);
    const QString iconName = qvariant_cast<QMimeType>(item->data(mimeTypeRole)).iconName();
    if (!iconName.isEmpty())
        item->setIcon(QIcon::fromTheme(iconName));
    item->setData(QVariant(true), iconQueriedRole);
    return item->icon();
}

QMimeType MimetypeModel::mimeType(const QModelIndex &index) const
{
    return qvariant_cast<QMimeType>(index.data(mimeTypeRole));
}

void MimetypeModel::populate()
{
    using Iterator = QList<QMimeType>::Iterator;

    QMimeDatabase mimeDatabase;
    QList<QMimeType> allTypes = mimeDatabase.allMimeTypes();

    // Move top level types to rear end of list, sort this partition,
    // create top level items and truncate the list.
    auto end = allTypes.end();
    auto topLevelStart =
        std::stable_partition(allTypes.begin(), end,
                              [](const QMimeType &t) { return !t.parentMimeTypes().isEmpty(); });
    std::stable_sort(topLevelStart, end);
    for (Iterator it = topLevelStart; it != end; ++it) {
        const StandardItemList row = createRow(*it);
        appendRow(row);
        m_nameIndexHash.insert(it->name(), indexFromItem(row.constFirst()));
    }
    allTypes.erase(topLevelStart, end);

    while (!allTypes.isEmpty()) {
        // Find a type inheriting one that is already in the model.
        end = allTypes.end();
        auto nameIndexIt = m_nameIndexHash.constEnd();
        for (Iterator it = allTypes.begin(); it != end; ++it) {
            nameIndexIt = m_nameIndexHash.constFind(it->parentMimeTypes().constFirst());
            if (nameIndexIt != m_nameIndexHash.constEnd())
                break;
        }
        if (nameIndexIt == m_nameIndexHash.constEnd()) {
            break;
        }

        // Move types inheriting the parent type to rear end of list, sort this partition,
        // append the items to parent and truncate the list.
        const QString &parentName = nameIndexIt.key();
        auto start =
            std::stable_partition(allTypes.begin(), end, [parentName](const QMimeType &t)
                                  { return !t.parentMimeTypes().contains(parentName); });
        std::stable_sort(start, end);
        QStandardItem *parentItem = itemFromIndex(nameIndexIt.value());
        for (Iterator it = start; it != end; ++it) {
            const StandardItemList row = createRow(*it);
            parentItem->appendRow(row);
            m_nameIndexHash.insert(it->name(), indexFromItem(row.constFirst()));
        }
        allTypes.erase(start, end);
    }
}

QTextStream &operator<<(QTextStream &stream, const QStringList &list)
{
    stream << list.join(QLatin1String(", "));
    return stream;
}

QString MimetypeModel::formatMimeTypeInfo(const QMimeType &t)
{
    QString result;
    QTextStream str(&result);
    str << "<html><head/><body><h3><center>" << t.name() << "</center></h3><br><table>";

    const QStringList &aliases = t.aliases();
    if (!aliases.isEmpty())
        str << tr("<tr><td>Aliases:</td><td>") << aliases;

    str << "</td></tr>"
        << tr("<tr><td>Comment:</td><td>") << t.comment() << "</td></tr>"
        << tr("<tr><td>Icon name:</td><td>") << t.iconName() << "</td></tr>"
        << tr("<tr><td>Generic icon name:</td><td>") << t.genericIconName() << "</td></tr>";

    const QString &filter = t.filterString();
    if (!filter.isEmpty())
        str << tr("<tr><td>Filter:</td><td>") << t.filterString() << "</td></tr>";

    const QStringList &patterns = t.globPatterns();
    if (!patterns.isEmpty())
        str << tr("<tr><td>Glob patterns:</td><td>") << patterns << "</td></tr>";

    const QStringList &parentMimeTypes = t.parentMimeTypes();
    if (!parentMimeTypes.isEmpty())
        str << tr("<tr><td>Parent types:</td><td>") << t.parentMimeTypes() << "</td></tr>";

    QStringList suffixes = t.suffixes();
    if (!suffixes.isEmpty()) {
        str << tr("<tr><td>Suffixes:</td><td>");
        const QString &preferredSuffix = t.preferredSuffix();
        if (!preferredSuffix.isEmpty()) {
            suffixes.removeOne(preferredSuffix);
            str << "<b>" << preferredSuffix << "</b> ";
        }
        str << suffixes << "</td></tr>";
    }
    str << "</table></body></html>";
    return result;
}
