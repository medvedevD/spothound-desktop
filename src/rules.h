#ifndef RULES_H
#define RULES_H

#include <QPair>
#include <QString>
#include <QStringList>

namespace Rules {
    QString norm(QString s);
    QPair<int, QString> score(const QString& name, const QString& descr,
                              bool hasSite, const QStringList& keywords);
}

#endif // RULES_H
