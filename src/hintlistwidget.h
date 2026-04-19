#pragma once
#include <QListWidget>
#include <QPainter>

class HintListWidget : public QListWidget {
public:
    explicit HintListWidget(QWidget* parent = nullptr) : QListWidget(parent) {}
    void setHint(const QString& h) { m_hint = h; viewport()->update(); }
protected:
    void paintEvent(QPaintEvent* e) override {
        QListWidget::paintEvent(e);
        if (count() == 0 && !m_hint.isEmpty()) {
            QPainter p(viewport());
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap, m_hint);
        }
    }
private:
    QString m_hint;
};
