#include "rules.h"
#include "core/rules.h"

QString Rules::norm(QString s)
{
    return QString::fromStdString(core::Rules::norm(s.toStdString()));
}

QPair<int, QString> Rules::score(const QString& name, const QString& descr,
                                  bool hasSite, const QStringList& keywords)
{
    std::vector<std::string> kws;
    kws.reserve(static_cast<size_t>(keywords.size()));
    for (const auto& k : keywords)
        kws.push_back(k.toStdString());
    auto [s, why] = core::Rules::score(name.toStdString(), descr.toStdString(), hasSite, kws);
    return {s, QString::fromStdString(why)};
}
