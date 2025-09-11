#ifndef PLACEROW_H
#define PLACEROW_H

#include <QString>

struct PlaceRow {
    QString source;
    QString query;
    QString name;
    QString address;
    QString phone;
    QString site;
    QString descr;
    int score = 0;
    QString why;
};
#endif // PLACEROW_H
