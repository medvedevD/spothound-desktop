#include "stopwordsstore.h"

#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QDebug>


StopWordsStore::StopWordsStore(const QString& p, QObject* parent)
    : QObject(parent), m_path(p)
{
    if (!load()) {
        rebuild();
        emit changed();
    }
}

void StopWordsStore::rebuild() {
    // стоп-слова
    QStringList esc;
    for (QString s : m_list) { s=s.trimmed(); if(!s.isEmpty()) esc << QRegularExpression::escape(s); }
    const QString stopCore = esc.join("|");
    const QString stopPat = stopCore.isEmpty()
        ? QStringLiteral("(?!x)x")
        : QStringLiteral("(^|[^\\p{L}\\p{N}_])(?:%1)($|[^\\p{L}\\p{N}_])").arg(stopCore);
    m_rxStop = QRegularExpression(stopPat,
        QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::CaseInsensitiveOption);

}

bool StopWordsStore::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) { rebuild(); return false; }
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
    if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return false;
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
    QString norm = text;
    norm.replace(QChar(0x0451), QChar(0x0435));            // ё→е
    return m_rxStop.isValid() && m_rxStop.match(norm).hasMatch();
}

bool StopWordsStore::matchesRow(const PlaceRow& r) const
{
    // общий текст
    QString blob = (r.name + " " + r.descr + " " + r.address + " " + r.site).toLower();

    qDebug() << "blob" << blob;
    blob.replace(QChar(0x0451), QChar(0x0435));            // ё→е
    if (matches(blob)) return true;

    // домены сайтов из r.site
    QString hosts;
    const auto parts = r.site.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QUrl u(part.trimmed());
        QString h = u.host().toLower();
        h.replace(QChar(0x0451), QChar(0x0435));
        if (!h.isEmpty()) { hosts += ' '; hosts += h; }
    }
    return matches(hosts);
}
