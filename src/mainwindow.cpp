#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "hintlistwidget.h"

#include "scrapetask.h"
#include "yandexscraper.h"
#include "twogisscraper.h"
#include "googlemapsscraper.h"

#include <QWebEngineProfile>
#include <QMenu>
#include <memory>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QKeyEvent>

static const char* kSpinnerHtml = R"(<!DOCTYPE html><html><head><style>
body{margin:0;display:flex;align-items:center;justify-content:center;
     height:100vh;background:#1e1e1e;font-family:sans-serif;color:#999;}
.spinner{width:32px;height:32px;border:3px solid #333;border-top-color:#5b9bd5;
         border-radius:50%;animation:spin 0.8s linear infinite;}
@keyframes spin{to{transform:rotate(360deg)}}
.wrap{display:flex;flex-direction:column;align-items:center;gap:12px;}
</style></head><body>
<div class="wrap"><div class="spinner"></div><span>Инициализация...</span></div>
</body></html>)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupStatusBar();
    setupTableView();
    setupBrowserPanel();
    setupKeywordsList();
    setupStopWordsList();
    setupResultsPage();

    auto updateGridLabel = [this](int n) {
        ui->gridLabel->setText(QString("Сетка %1×%1:").arg(n));
    };
    connect(ui->gridSize, qOverload<int>(&QSpinBox::valueChanged), this, updateGridLabel);
    updateGridLabel(ui->gridSize->value());

    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::onStart);

    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::setupStatusBar()
{
    m_phaseLbl = new QLabel(this);
    m_progress = new QProgressBar(this);
    m_progress->setMinimumWidth(320);
    m_progress->setTextVisible(true);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setFormat(QString());

    statusBar()->addWidget(m_phaseLbl);
    statusBar()->addWidget(m_progress);
    m_progress->hide();
    statusBar()->hide();
}

void MainWindow::setupTableView()
{
    m_model = new PlacesModel(this);
    ui->tableView->setModel(m_model);
    ui->tableView->setStyleSheet("QTableView { border: 1px solid rgba(255,255,255,38); }");

    ui->tableView->setColumnHidden(PlacesModel::Score, true);
    ui->tableView->setColumnHidden(PlacesModel::Why, true);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTableContextMenu);

    connect(ui->exportCsv, &QPushButton::clicked, this, [this] {
        const QString fn = QFileDialog::getSaveFileName(this, "Экспорт в CSV",
                             "results.csv", "CSV (*.csv)");
        if (fn.isEmpty()) return;
        m_model->exportCsv(fn);
    });

    connect(ui->exportXlsx, &QPushButton::clicked, this, [this] {
        const QString fn = QFileDialog::getSaveFileName(this, "Экспорт в Excel",
                             "results.xlsx", "Excel (*.xlsx)");
        if (fn.isEmpty()) return;
        m_model->exportXlsx(fn);
    });
}

void MainWindow::setupBrowserPanel()
{
    m_profile = new QWebEngineProfile(this);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->setPersistentStoragePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/profile");
    m_profile->setHttpUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122 Safari/537.36");
    m_profile->setHttpAcceptLanguage("ru-RU,ru;q=0.9,en-US;q=0.7");

    auto* browserLayout = new QVBoxLayout(ui->browserContainer);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);

    m_loadBar = new QProgressBar(ui->browserContainer);
    m_loadBar->setRange(0, 100);
    m_loadBar->setValue(0);
    m_loadBar->setTextVisible(false);
    m_loadBar->setFixedHeight(3);
    m_loadBar->setStyleSheet(
        "QProgressBar { background: transparent; border: none; }"
        "QProgressBar::chunk { background: #5b9bd5; }");
    m_loadBar->setVisible(false);
    browserLayout->addWidget(m_loadBar);

    m_view = new QWebEngineView(ui->browserContainer);
    m_view->page()->setBackgroundColor(QColor("#1e1e1e"));
    browserLayout->addWidget(m_view);

    connect(m_view, &QWebEngineView::loadStarted, this, [this] {
        m_loadBar->setValue(0);
        m_loadBar->setVisible(true);
    });
    connect(m_view, &QWebEngineView::loadProgress, m_loadBar, &QProgressBar::setValue);
    connect(m_view, &QWebEngineView::loadFinished, this, [this] {
        m_loadBar->setVisible(false);
    });

    ui->browserContainer->hide();
}

void MainWindow::setupKeywordsList()
{
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
}

void MainWindow::setupStopWordsList()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    m_stopStore = new StopWordsStore(path + "/stopwords.txt", this);

    ui->stopWordsList->setHint("Нет стоп-слов");
    ui->stopWordsList->installEventFilter(this);

    for (const QString& w : m_stopStore->list())
        ui->stopWordsList->addItem(w);

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
}

void MainWindow::setupResultsPage()
{
    connect(ui->previewCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            ui->browserContainer->setVisible(true);
            if (!m_splitterSizes.isEmpty())
                ui->browserSplitter->setSizes(m_splitterSizes);
        } else {
            m_splitterSizes = ui->browserSplitter->sizes();
            ui->browserContainer->setVisible(false);
        }
    });

    connect(ui->btnNewSearch, &QPushButton::clicked, this, [this] {
        if (m_currentScraper) m_currentScraper->reset();
        m_model->clear();
        m_phaseLbl->setText(QString());
        statusBar()->hide();
        m_progress->hide();
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_progress->setFormat(QString());
        ui->previewCheck->setChecked(false);
        ui->stackedWidget->setCurrentIndex(0);
    });

    connect(ui->btnClear, &QPushButton::clicked, this, [this] {
        if (m_currentScraper) m_currentScraper->reset();
        m_model->clear();
        m_phaseLbl->setText(QString());
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_progress->setFormat(QString());
    });
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
    const QString cityRaw = ui->cityInput->text().trimmed();
    const QString city    = cityRaw.isEmpty() ? QStringLiteral("Москва") : cityRaw;
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

    static const QStringList sourceNames = {"Яндекс.Карты", "2ГИС", "Google Maps"};
    QString summary = sourceNames.value(source) + " · " + city + " · " + query;
    if (!keywords.isEmpty()) summary += " · " + keywords.join(", ");
    ui->searchSummaryLabel->setText(summary);

    m_progress->show();
    statusBar()->show();
    ui->stackedWidget->setCurrentIndex(1);

    auto setViewPage = [this](QWebEnginePage* page) {
        QWebEnginePage* old = m_view->page();
        if (old && old->parent() == m_view)
            delete old;
        m_view->setPage(page);
        page->setParent(m_view);
    };

    connect(scraper, &ScrapeTask::captchaRequested, this, [this, setViewPage](QWebEnginePage* page){
        setViewPage(page);
        ui->previewCheck->setChecked(true);
    });

    connect(scraper, &ScrapeTask::preview, this, [this, setViewPage](QWebEnginePage* page, const QString&){
        if (!ui->previewCheck->isChecked()) return;
        setViewPage(page);
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

    auto statsHolder = std::make_shared<ScrapeStats>();
    connect(scraper, &ScrapeTask::statsReady, this, [statsHolder](ScrapeStats s){
        *statsHolder = s;
    });

    connect(scraper, &ScrapeTask::finishedAll, this, [this, statsHolder]{
        const ScrapeStats& s = *statsHolder;
        if (s.cardCount > 0) {
            m_phaseLbl->setText(QString("Готово | Сбор: %1с | %2 карт за %3с (avg %4с/карт)")
                .arg(s.collectionMs / 1000.0, 0, 'f', 1)
                .arg(s.cardCount)
                .arg(s.parsingMs / 1000.0, 0, 'f', 1)
                .arg(s.avgCardMs() / 1000.0, 0, 'f', 1));
        } else {
            m_phaseLbl->setText("Готово");
        }
        m_progress->setFormat(QString());
        m_progress->setValue(m_progress->maximum());
    });

    connect(scraper, &ScrapeTask::result, m_model, &PlacesModel::addRow);
    connect(scraper, &ScrapeTask::finished, this, [this, scraper]{
        if (m_currentScraper == scraper) m_currentScraper = nullptr;
        scraper->deleteLater();
    });

    m_view->setHtml(kSpinnerHtml);

    scraper->start();
}

void MainWindow::onTableContextMenu(const QPoint& pos) {
    auto* tv = ui->tableView;
    const QModelIndex idx = tv->indexAt(pos);
    if (!idx.isValid()) return;

    QMenu menu(this);
    QAction* aCell = menu.addAction("Копировать ячейку");
    QAction* aRow  = menu.addAction("Копировать строку");
    QAction* aCol  = menu.addAction("Копировать столбец");
    QAction* aDel  = menu.addAction("Удалить строку");

    QAction* act = menu.exec(tv->viewport()->mapToGlobal(pos));
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
