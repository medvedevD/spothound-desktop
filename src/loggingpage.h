#ifndef LOGGINGPAGE_H
#define LOGGINGPAGE_H


#include <QWebEnginePage>
#include <QDebug>

class LoggingPage : public QWebEnginePage {
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override {
        Q_UNUSED(level);
        qDebug() << "[JS]" << sourceID << ":" << lineNumber << message;
    }
};

#endif // LOGGINGPAGE_H
