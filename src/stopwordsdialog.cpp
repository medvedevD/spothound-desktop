#include "stopwordsdialog.h"
#include "ui_stopwordsdialog.h"

#include <QFile>
#include <QFileDialog>
#include <QTextStream>

StopWordsDialog::StopWordsDialog(StopWordsStore *store, QWidget *parent) :
    QDialog(parent),
    m_store(store), m_model(new QStringListModel(this)),
    ui(new Ui::StopWordsDialog)
{
    ui->setupUi(this);

    setWindowTitle("Stop-words");

    ui->listView->setModel(m_model);

    rebuild();

    connect(ui->addBtn, &QPushButton::clicked, this, [this]{
        auto l = m_model->stringList();
        const QString s = ui->lineEdit->text().trimmed();
        if (!s.isEmpty() && !l.contains(s, Qt::CaseInsensitive)) {
            l << s; m_model->setStringList(l); ui->lineEdit->clear();
            m_store->setList(l);
        }
    });
    connect(ui->deteleBtn, &QPushButton::clicked, this, [this]{
        const auto idx = ui->listView->currentIndex();
        if (!idx.isValid()) return;
        auto l = m_model->stringList();
        l.removeAt(idx.row());
        m_model->setStringList(l);
        m_store->setList(l);
    });
    connect(ui->importBtn, &QPushButton::clicked, this, [this]{
        const auto fn = QFileDialog::getOpenFileName(this, "Импорт", QString(), "TXT (*.txt);;All (*)");
        if (fn.isEmpty()) return;
        QFile f(fn); if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return;
        QStringList l = m_model->stringList();
        QTextStream ts(&f); ts.setCodec("UTF-8");
        while (!ts.atEnd()) l << ts.readLine().trimmed();
        l.removeAll(QString()); l.removeDuplicates();
        m_model->setStringList(l);
        m_store->setList(l);
    });
    connect(ui->exportBtn, &QPushButton::clicked, this, [this]{
        const auto fn = QFileDialog::getSaveFileName(this, "Экспорт", "stopwords.txt", "TXT (*.txt)");
        if (fn.isEmpty()) return;
        QFile f(fn); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream ts(&f); ts.setCodec("UTF-8");
        for (const auto& s : m_model->stringList()) ts << s << "\n";
    });

    connect(ui->closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_store, &StopWordsStore::changed, this, &StopWordsDialog::rebuild);
}

StopWordsDialog::~StopWordsDialog()
{
    delete ui;
}

void StopWordsDialog::rebuild()
{
    m_model->setStringList(m_store->list());
}
