#include "stopwordsstore.h"

#include <QFile>
#include <QTextStream>

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
    if (m_path.isEmpty()) { rebuild(); return false; }
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
    if (m_path.isEmpty()) return true;
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

bool StopWordsStore::matchesRow(const core::PlaceRow& r) const
{
    return m_filter.matchesPlace(r);
}
