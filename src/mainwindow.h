#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "placesmodel.h"
#include "stopwordsstore.h"

#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSpinBox>
#include <QWebEngineView>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ScrapeTask;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void onStart();
    void onTableContextMenu(const QPoint &pos);

private:
    Ui::MainWindow *ui;
    PlacesModel *m_model = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    QWebEngineView *m_view = nullptr;
StopWordsStore *m_stopStore = nullptr;
    ScrapeTask *m_currentScraper = nullptr;

    QList<int> m_splitterSizes;

    QLabel* m_phaseLbl = nullptr;
    QProgressBar* m_progress = nullptr;
};

#endif // MAINWINDOW_H
