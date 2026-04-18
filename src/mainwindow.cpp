#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "hintlistwidget.h"

#include "scrapetask.h"
#include "yandexscraper.h"
#include "twogisscraper.h"
#include "googlemapsscraper.h"

#include <QWebEngineProfile>
#include <QStandardPaths>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_phaseLbl = new QLabel(this);
    m_progress = new QProgressBar(this);
    m_progress->setMinimumWidth(320);
    m_progress->setTextVisible(true);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setFormat(QString());

    statusBar()->addWidget(m_phaseLbl);
    statusBar()->addWidget(m_progress);

    m_model = new PlacesModel(this);
    ui->tableView->setModel(m_model);

    ui->tableView->setColumnHidden(PlacesModel::Score, true);
    ui->tableView->setColumnHidden(PlacesModel::Why, true);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTableContextMenu);

    connect(ui->exportCsv, &QPushButton::clicked, this, [this] {
        m_model->exportCsv("1.csv");
    });

    m_profile = new QWebEngineProfile(this);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->setPersistentStoragePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/profile");
    m_profile->setHttpUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122 Safari/537.36");
    m_profile->setHttpAcceptLanguage("ru-RU,ru;q=0.9,en-US;q=0.7");

    YandexScraper::setupProfile(m_profile);

    m_view = new QWebEngineView(nullptr);
    m_view->setAttribute(Qt::WA_QuitOnClose, false);
    m_view->setWindowFlag(Qt::Tool, true);
    m_view->setWindowModality(Qt::NonModal);
    m_view->resize(1100, 800);
    m_view->hide();

    auto updateGridLabel = [this](int n) {
        ui->gridLabel->setText(QString("Сетка %1×%1:").arg(n));
    };
    connect(ui->gridSize, qOverload<int>(&QSpinBox::valueChanged), this, updateGridLabel);
    updateGridLabel(ui->gridSize->value());

    // Keywords list
    ui->keywordsList->setHint("Нет ключевых слов");
    ui->keywordsList->installEventFilter(this);

    auto addKeyword = [this] {
        const QString word = ui->keywordInput->text().trimmed();
        if (word.isEmpty()) return;
        for (int i = 0; i < ui->keywordsList->count(); ++i)
            if (ui->keywordsList->item(i)->text().compare(word, Qt::CaseInsensitive) == 0) {
                ui->keywordInput->clear();
                return;
            }
        ui->keywordsList->addItem(word);
        ui->keywordInput->clear();
    };
    connect(ui->btnAddKeyword, &QPushButton::clicked, this, addKeyword);
    connect(ui->keywordInput, &QLineEdit::returnPressed, this, addKeyword);
    connect(ui->btnRemoveKeyword, &QPushButton::clicked, this, [this] {
        delete ui->keywordsList->currentItem();
    });

    // Stop words list
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    m_stopStore = new StopWordsStore(path + "/stopwords.txt", this);

    ui->stopWordsList->setHint("Нет стоп-слов");
    ui->stopWordsList->installEventFilter(this);

    auto reloadStopWords = [this] {
        ui->stopWordsList->clear();
        for (const QString& w : m_stopStore->list())
            ui->stopWordsList->addItem(w);
    };
    reloadStopWords();

    auto addStopWord = [this] {
        const QString word = ui->stopWordInput->text().trimmed();
        if (word.isEmpty()) return;
        QStringList words = m_stopStore->list();
        if (words.contains(word, Qt::CaseInsensitive)) { ui->stopWordInput->clear(); return; }
        words << word;
        m_stopStore->setList(words);
        ui->stopWordsList->addItem(word);
        ui->stopWordInput->clear();
    };
    connect(ui->btnAddStopWord, &QPushButton::clicked, this, addStopWord);
    connect(ui->stopWordInput, &QLineEdit::returnPressed, this, addStopWord);
    connect(ui->btnRemoveStopWord, &QPushButton::clicked, this, [this] {
        const int row = ui->stopWordsList->currentRow();
        if (row < 0) return;
        QStringList words = m_stopStore->list();
        words.removeAt(row);
        m_stopStore->setList(words);
        delete ui->stopWordsList->currentItem();
    });

    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::onStart);

    connect(ui->btnNewSearch, &QPushButton::clicked, this, [this] {
        if (m_currentScraper) m_currentScraper->reset();
        m_model->clear();
        m_phaseLbl->setText(QString());
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_progress->setFormat(QString());
        if (m_view) m_view->hide();
        ui->stackedWidget->setCurrentIndex(0);
    });

    connect(ui->exportXlsx, &QPushButton::clicked, this, [this]{
        const QString fn = QFileDialog::getSaveFileName(this, "Экспорт в Excel",
                             "results.xlsx", "Excel (*.xlsx)");
        if (fn.isEmpty()) return;
        m_model->exportXlsx(fn);
    });

    connect(ui->btnClear, &QPushButton::clicked, this, [this]{
        if (m_currentScraper) m_currentScraper->reset();
        m_model->clear();
        m_phaseLbl->setText(QString());
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_progress->setFormat(QString());
        if (m_view) m_view->hide();
    });

    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            if (obj == ui->keywordsList) {
                delete ui->keywordsList->currentItem();
                return true;
            }
            if (obj == ui->stopWordsList) {
                const int row = ui->stopWordsList->currentRow();
                if (row >= 0) {
                    QStringList words = m_stopStore->list();
                    words.removeAt(row);
                    m_stopStore->setList(words);
                    delete ui->stopWordsList->currentItem();
                }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::onStart()
{
    const QString query = ui->searchRequest->text().trimmed();
    const QString city  = ui->cityInput->text().trimmed().isEmpty()
                          ? QStringLiteral("Москва")
                          : ui->cityInput->text().trimmed();
    if (query.isEmpty()) return;

    QStringList keywords;
    for (int i = 0; i < ui->keywordsList->count(); ++i)
        keywords << ui->keywordsList->item(i)->text();

    if (m_currentScraper) m_currentScraper->reset();

    ScrapeTask* scraper = nullptr;
    const int source = ui->sourceCombo->currentIndex();
    switch (source) {
    case 0: scraper = new YandexScraper(query, city, m_profile, m_stopStore, keywords, ui->gridSize->value(), this); break;
    case 1: scraper = new TwoGisScraper(query, city, m_profile, m_stopStore, keywords, this); break;
    case 2: scraper = new GoogleMapsScraper(query, city, m_profile, m_stopStore, keywords, this); break;
    default: return;
    }

    m_currentScraper = scraper;

    const QStringList sourceNames = {"Яндекс.Карты", "2ГИС", "Google Maps"};
    QString summary = sourceNames.value(source) + " · " + city + " · " + query;
    if (!keywords.isEmpty()) summary += " · " + keywords.join(", ");
    ui->searchSummaryLabel->setText(summary);

    ui->stackedWidget->setCurrentIndex(1);

    connect(scraper, &ScrapeTask::captchaRequested, this, [this](QWebEnginePage* page){
        m_view->setPage(page);
        m_view->show();
        m_view->raise();
        m_view->activateWindow();
    });

    connect(scraper, &ScrapeTask::preview, this, [this](QWebEnginePage* page, const QString& title){
        if (!m_preview) return;
        m_view->setPage(page);
        m_view->setWindowTitle(title);
        m_view->show();
        m_view->raise();
        m_view->activateWindow();
    });

    connect(scraper, &ScrapeTask::phaseChanged, this, [this](const QString& p){
        m_phaseLbl->setText(p);
    });

    connect(scraper, &ScrapeTask::gridProgress, this, [this](int total, int done){
        m_progress->setRange(0, qMax(1, total));
        m_progress->setValue(done);
        m_progress->setFormat(QString("Зоны: %1/%2 (%p%)").arg(done).arg(total));
    });

    connect(scraper, &ScrapeTask::queueSized, this, [this](int total){
        m_progress->setRange(0, qMax(1, total));
        m_progress->setValue(0);
        m_progress->setFormat(QString("Карточки: 0/%1 (0%)").arg(total));
    });

    connect(scraper, &ScrapeTask::parseProgress, this, [this](int total, int done){
        m_progress->setRange(0, qMax(1, total));
        m_progress->setValue(done);
        m_progress->setFormat(QString("Карточки: %1/%2 (%p%)").arg(done).arg(total));
    });

    connect(scraper, &ScrapeTask::finishedAll, this, [this]{
        m_phaseLbl->setText("Готово");
        m_progress->setFormat(QString());
        m_progress->setValue(m_progress->maximum());
    });

    connect(scraper, &ScrapeTask::result, m_model, &PlacesModel::addRow);
    connect(scraper, &ScrapeTask::finished, this, [this, scraper]{
        if (m_currentScraper == scraper) m_currentScraper = nullptr;
        scraper->deleteLater();
    });

    scraper->start();
}

void MainWindow::onTableContextMenu(const QPoint& pos) {
    auto* tv = ui->tableView;
    const QModelIndex idx = tv->indexAt(pos);
    if (!idx.isValid()) return;

    QMenu m(this);
    QAction* aCell = m.addAction("Копировать ячейку");
    QAction* aRow  = m.addAction("Копировать строку");
    QAction* aCol  = m.addAction("Копировать столбец");
    QAction* aDel  = m.addAction("Удалить строку");

    QAction* act = m.exec(tv->viewport()->mapToGlobal(pos));
    if (!act) return;

    auto* mdl = tv->model();
    QString out;

    if (act == aCell) {
        out = mdl->data(idx, Qt::DisplayRole).toString();
    } else if (act == aRow) {
        const int r = idx.row();
        QStringList cells;
        for (int c = 0; c < mdl->columnCount(); ++c)
            cells << mdl->data(mdl->index(r, c), Qt::DisplayRole).toString();
        out = cells.join("\t");
    } else if (act == aCol) {
        const int c = idx.column();
        QStringList lines;
        for (int r = 0; r < mdl->rowCount(); ++r)
            lines << mdl->data(mdl->index(r, c), Qt::DisplayRole).toString();
        out = lines.join("\n");
    } else if (act == aDel) {
        mdl->removeRow(idx.row());
        return;
    }

    QGuiApplication::clipboard()->setText(out);
}
