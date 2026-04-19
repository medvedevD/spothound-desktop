#ifndef STOPWORDSSTORE_H
#define STOPWORDSSTORE_H

#include "core/place_row.h"
#include "core/stop_words_filter.h"

#include <QObject>
#include <QStringList>

class StopWordsStore : public QObject {
    Q_OBJECT
public:
    explicit StopWordsStore(const QString& filePath, QObject* parent=nullptr);

    const QStringList& list() const { return m_list; }
    void setList(QStringList l);
    bool load();
    bool save() const;
    bool matches(const QString& text) const;
    bool matchesRow(const core::PlaceRow& r) const;

signals:
    void changed();

private:
    QString m_path;
    QStringList m_list;
    void rebuild();
    core::StopWordsFilter m_filter;
};

#endif // STOPWORDSSTORE_H
