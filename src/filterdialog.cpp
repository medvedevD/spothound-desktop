#include "filterdialog.h"
#include "hintlistwidget.h"
#include "stopwordsstore.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QKeyEvent>

FilterDialog::FilterDialog(QStringList keywords, StopWordsStore* stopStore, QWidget* parent)
    : QDialog(parent)
    , m_stopStore(stopStore)
{
    setWindowTitle("Фильтры");
    setMinimumSize(480, 400);

    auto* tabs = new QTabWidget(this);

    // --- Tab: Keywords ---
    auto* kwTab = new QWidget;
    auto* kwLayout = new QVBoxLayout(kwTab);

    auto* kwLabel = new QLabel("Ключевые слова для скоринга:");
    m_keywordsList = new HintListWidget;
    m_keywordsList->setHint("Нет ключевых слов");
    m_keywordsList->installEventFilter(this);
    for (const QString& w : keywords)
        m_keywordsList->addItem(w);

    auto* kwInput = new QLineEdit;
    kwInput->setPlaceholderText("Новое слово...");
    auto* kwAdd = new QPushButton("Добавить");
    auto* kwDel = new QPushButton("Удалить");

    auto* kwRow = new QHBoxLayout;
    kwRow->addWidget(kwInput);
    kwRow->addWidget(kwAdd);
    kwRow->addWidget(kwDel);

    kwLayout->addWidget(kwLabel);
    kwLayout->addWidget(m_keywordsList);
    kwLayout->addLayout(kwRow);

    connect(kwAdd, &QPushButton::clicked, this, [this, kwInput]{ addKeyword(kwInput); });
    connect(kwInput, &QLineEdit::returnPressed, this, [this, kwInput]{ addKeyword(kwInput); });
    connect(kwDel, &QPushButton::clicked, this, [this]{
        delete m_keywordsList->currentItem();
    });

    // --- Tab: Stop words ---
    auto* swTab = new QWidget;
    auto* swLayout = new QVBoxLayout(swTab);

    auto* swLabel = new QLabel("Результаты с этими словами будут скрыты:");
    m_stopWordsList = new HintListWidget;
    m_stopWordsList->setHint("Нет стоп-слов");
    m_stopWordsList->installEventFilter(this);
    for (const QString& w : stopStore->list())
        m_stopWordsList->addItem(w);

    auto* swInput = new QLineEdit;
    swInput->setPlaceholderText("Новое слово...");
    auto* swAdd = new QPushButton("Добавить");
    auto* swDel = new QPushButton("Удалить");

    auto* swRow = new QHBoxLayout;
    swRow->addWidget(swInput);
    swRow->addWidget(swAdd);
    swRow->addWidget(swDel);

    swLayout->addWidget(swLabel);
    swLayout->addWidget(m_stopWordsList);
    swLayout->addLayout(swRow);

    connect(swAdd, &QPushButton::clicked, this, [this, swInput]{ addStopWord(swInput); });
    connect(swInput, &QLineEdit::returnPressed, this, [this, swInput]{ addStopWord(swInput); });
    connect(swDel, &QPushButton::clicked, this, [this]{
        const int row = m_stopWordsList->currentRow();
        if (row < 0) return;
        QStringList words = m_stopStore->list();
        words.removeAt(row);
        m_stopStore->setList(words);
        delete m_stopWordsList->currentItem();
    });

    tabs->addTab(kwTab, "Ключевые слова");
    tabs->addTab(swTab, "Стоп-слова");

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto* main = new QVBoxLayout(this);
    main->addWidget(tabs);
    main->addWidget(box);
}

QStringList FilterDialog::keywords() const
{
    QStringList result;
    for (int i = 0; i < m_keywordsList->count(); ++i)
        result << m_keywordsList->item(i)->text();
    return result;
}

void FilterDialog::addKeyword(QLineEdit* input)
{
    const QString word = input->text().trimmed();
    if (word.isEmpty()) return;
    for (int i = 0; i < m_keywordsList->count(); ++i)
        if (m_keywordsList->item(i)->text().compare(word, Qt::CaseInsensitive) == 0) {
            input->clear();
            return;
        }
    m_keywordsList->addItem(word);
    input->clear();
}

void FilterDialog::addStopWord(QLineEdit* input)
{
    const QString word = input->text().trimmed();
    if (word.isEmpty()) return;
    QStringList words = m_stopStore->list();
    if (words.contains(word, Qt::CaseInsensitive)) { input->clear(); return; }
    words << word;
    m_stopStore->setList(words);
    m_stopWordsList->addItem(word);
    input->clear();
}

bool FilterDialog::eventFilter(QObject* obj, QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
            if (obj == m_keywordsList) {
                delete m_keywordsList->currentItem();
                return true;
            }
            if (obj == m_stopWordsList) {
                const int row = m_stopWordsList->currentRow();
                if (row >= 0) {
                    QStringList words = m_stopStore->list();
                    words.removeAt(row);
                    m_stopStore->setList(words);
                    delete m_stopWordsList->currentItem();
                }
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, e);
}
