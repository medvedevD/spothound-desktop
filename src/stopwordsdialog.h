#ifndef STOPWORDSDIALOG_H
#define STOPWORDSDIALOG_H

#include "stopwordsstore.h"

#include <QDialog>
#include <QStringListModel>

namespace Ui {
class StopWordsDialog;
}

class StopWordsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StopWordsDialog(StopWordsStore* store, QWidget *parent = nullptr);
    ~StopWordsDialog();

private:
    void rebuild();

private:
    Ui::StopWordsDialog *ui;
    StopWordsStore* m_store;
    QStringListModel* m_model;
};

#endif // STOPWORDSDIALOG_H
