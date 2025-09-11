#ifndef STOPWORDSSTORE_H
#define STOPWORDSSTORE_H

#include "placerow.h"

#include <QObject>
#include <QStringList>
#include <QRegularExpression>

class StopWordsStore : public QObject {
    Q_OBJECT
public:
    explicit StopWordsStore(const QString& filePath, QObject* parent=nullptr);

    const QStringList& list() const { return m_list; }
    void setList(QStringList l);        // заменяет список и сохраняет
    bool load();                        // читает из файла
    bool save() const;                  // пишет в файл
    bool matches(const QString& text) const;
    bool matchesRow(const PlaceRow& r) const;

signals:
    void changed();

private:
    QString m_path;
    QStringList m_list;
    void rebuild();

    QRegularExpression m_rxStop;
};

#endif // STOPWORDSSTORE_H
