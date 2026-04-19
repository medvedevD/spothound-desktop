#ifndef PLACESMODEL_H
#define PLACESMODEL_H

#include "core/place_row.h"
#include "core/place_row_json.h"

#include <QAbstractTableModel>
#include <vector>

class PlacesModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Col { Source, Query, Name, Address, Phone, Site, Score, Why, Description, Count };

    explicit PlacesModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addRow(core::PlaceRow row);
    void exportCsv(const QString& path) const;
    bool exportXlsx(const QString &path) const;
    void clear();

private:
    std::vector<core::PlaceRow> m_rows;
};

#endif // PLACESMODEL_H
