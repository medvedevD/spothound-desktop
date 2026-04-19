#include "stopwordsstore.h"

#include <QFile>
#include <QTextStream>
#include <QUrl>

StopWordsStore::StopWordsStore(const QString& p, QObject* parent)
    : QObject(parent), m_path(p)
{
    if (!load()) {
        rebuild();
        emit changed();
    }
}

void StopWordsStore::rebuild()
{
    std::vector<std::string> words;
    words.reserve(static_cast<size_t>(m_list.size()));
    for (const auto& s : m_list)
        words.push_back(s.toStdString());
    m_filter.setWords(std::move(words));
}

bool StopWordsStore::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { rebuild(); return false; }
    QStringList out;
    QTextStream ts(&f); ts.setCodec("UTF-8");
    while (!ts.atEnd()) out << ts.readLine();
    m_list = out;
    rebuild();
    emit changed();
    return true;
}

bool StopWordsStore::save() const
{
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&f); ts.setCodec("UTF-8");
    for (const auto& s : m_list) ts << s << "\n";
    return true;
}

void StopWordsStore::setList(QStringList l)
{
    m_list = l;
    m_list.removeAll(QString());
    for (auto& s : m_list) s = s.trimmed();
    m_list.removeDuplicates();
    rebuild();
    save();
    emit changed();
}

bool StopWordsStore::matches(const QString& text) const
{
    return m_filter.matches(text.toStdString());
}

bool StopWordsStore::matchesRow(const PlaceRow& r) const
{
    const std::string blob =
        (r.name + " " + r.descr + " " + r.address + " " + r.site).toStdString();
    if (m_filter.matches(blob)) return true;

    std::string hosts;
    const auto parts = r.site.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QUrl u(part.trimmed());
        const std::string h = u.host().toStdString();
        if (!h.empty()) { hosts += ' '; hosts += h; }
    }
    return m_filter.matches(hosts);
}
