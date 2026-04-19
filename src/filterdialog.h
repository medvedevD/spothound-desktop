#pragma once
#include <QDialog>
#include <QStringList>

class StopWordsStore;
class HintListWidget;
class QLineEdit;

class FilterDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterDialog(QStringList keywords, StopWordsStore* stopStore, QWidget* parent = nullptr);
    QStringList keywords() const;

private:
    HintListWidget* m_keywordsList  = nullptr;
    HintListWidget* m_stopWordsList = nullptr;
    StopWordsStore* m_stopStore     = nullptr;

    void addKeyword(QLineEdit* input);
    void addStopWord(QLineEdit* input);
    bool eventFilter(QObject* obj, QEvent* e) override;
};
