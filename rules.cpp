#include "rules.h"

#include <QRegularExpression>

QString Rules::norm(QString s)
{
    s=s.toLower().trimmed();
    s.replace(QChar(0x0451), QChar(0x0435));
    s.replace(QRegularExpression("\\s+"), " ");
    return s;
}

QPair<int,QString> Rules::score(const QString& name, const QString& descr,
                                bool hasSite, const QStringList& keywords)
{
    const QString text = norm(name + " " + descr);
    int s=0; QStringList why;
    for (const auto& kw: keywords) {
        const QString k = kw.trimmed();
        if (!k.isEmpty() && text.contains(k)) { ++s; why << "kw:"+k; }
    }
    if (hasSite) { ++s; why << "site"; }
    return {s, why.join(", ")};
}
QString Rules::hashKey(const PlaceRow& r)
{
    return QCryptographicHash::hash((norm(r.name)+"|"+norm(r.address)).toUtf8(), QCryptographicHash::Sha1).toHex();
}
