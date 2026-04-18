#ifndef RULES_H
#define RULES_H

#include "placerow.h"

#include <QPair>
#include <QString>
#include <QCryptographicHash>
#include <QStringList>


namespace Rules {
QString norm(QString s);
QPair<int,QString> score(const QString& name, const QString& descr,
                         bool hasSite, const QStringList& keywords);
QString hashKey(const PlaceRow& r);
}


#endif // RULES_H
