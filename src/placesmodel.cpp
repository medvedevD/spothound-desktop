#include "placesmodel.h"
#include "core/csv_export.h"
#include "place_row_convert.h"

#include <QFile>
#include <QTextStream>

#include <xlsxdocument.h>
#include <xlsxformat.h>
#include <xlsxworksheet.h>
#include <xlsxcellrange.h>

PlacesModel::PlacesModel(QObject* parent) : QAbstractTableModel(parent) {}

int PlacesModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(m_rows.size());
}

int PlacesModel::columnCount(const QModelIndex&) const {
    return Count;
}

QVariant PlacesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const core::PlaceRow& r = m_rows.at(static_cast<size_t>(index.row()));
    switch (index.column()) {
    case Source:      return QString::fromStdString(r.source);
    case Query:       return QString::fromStdString(r.query);
    case Name:        return QString::fromStdString(r.name);
    case Address:     return QString::fromStdString(r.address);
    case Phone:       return QString::fromStdString(r.phone);
    case Site:        return QString::fromStdString(r.site);
    case Score:       return r.score;
    case Why:         return QString::fromStdString(r.why);
    case Description: return QString::fromStdString(r.descr);
    }
    return {};
}

QVariant PlacesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case Source:      return "Источник";
    case Query:       return "Запрос";
    case Name:        return "Название";
    case Address:     return "Адрес";
    case Phone:       return "Телефон";
    case Site:        return "Сайт";
    case Score:       return "Баллы";
    case Why:         return "Причины";
    case Description: return "Описание";
    }
    return {};
}

bool PlacesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    if (row < 0 || static_cast<size_t>(row + count) > m_rows.size())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    m_rows.erase(m_rows.begin() + row, m_rows.begin() + row + count);
    endRemoveRows();

    return true;
}

Qt::ItemFlags PlacesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractTableModel::flags(index) | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void PlacesModel::addRow(const PlaceRow& row) {
    beginInsertRows(QModelIndex(), static_cast<int>(m_rows.size()), static_cast<int>(m_rows.size()));
    m_rows.push_back(toCore(row));
    endInsertRows();
}

void PlacesModel::exportCsv(const QString& path) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    ts << QString::fromStdString(core::toCsv(m_rows));
}

bool PlacesModel::exportXlsx(const QString& path) const
{
    QXlsx::Document xlsx;

    QXlsx::Format head; head.setFontBold(true);
    head.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    head.setVerticalAlignment(QXlsx::Format::AlignTop);
    head.setTextWrap(true);
    head.setBorderStyle(QXlsx::Format::BorderThin);

    QXlsx::Format cell; cell.setVerticalAlignment(QXlsx::Format::AlignTop);
    cell.setTextWrap(true);
    cell.setBorderStyle(QXlsx::Format::BorderThin);

    const QVector<int> exportCols = {
        Source, Query, Name, Address, Phone, Site, Description
    };

    for (int i = 0; i < exportCols.size(); ++i) {
        const int c = exportCols[i];
        xlsx.write(1, i + 1, headerData(c, Qt::Horizontal, Qt::DisplayRole).toString(), head);
    }

    const int rows = rowCount();
    QVector<int> maxLen(exportCols.size(), 0);
    for (int r = 0; r < rows; ++r) {
        for (int i = 0; i < exportCols.size(); ++i) {
            const int c = exportCols[i];
            const QString v = index(r, c).data(Qt::DisplayRole).toString();
            xlsx.write(r + 2, i + 1, v, cell);
            maxLen[i] = qMax(maxLen[i], v.size());
        }
    }
    for (int i = 0; i < exportCols.size(); ++i) {
        const int ch = qBound(12, int(maxLen[i] * 0.9), 60);
        xlsx.setColumnWidth(i + 1, ch);
    }

    return xlsx.saveAs(path);
}

void PlacesModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}
