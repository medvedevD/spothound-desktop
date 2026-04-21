#ifndef CAPTCHAAWAREPAGE_H
#define CAPTCHAAWAREPAGE_H

#include <QWebEnginePage>
#include <QColor>
#include <functional>

class CaptchaAwarePage : public QWebEnginePage {
    Q_OBJECT
public:
    using CaptchaPredicate = std::function<bool(const QUrl&)>;

    explicit CaptchaAwarePage(QWebEngineProfile* profile,
                               CaptchaPredicate pred,
                               QObject* parent = nullptr)
        : QWebEnginePage(profile, parent)
        , m_pred(std::move(pred))
    {
        setBackgroundColor(QColor("#1e1e1e"));
    }

signals:
    void captchaNeeded(const QUrl&);

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType t, bool isMain) override {
        if (isMain && m_pred(url))
            emit captchaNeeded(url);
        return QWebEnginePage::acceptNavigationRequest(url, t, isMain);
    }

private:
    CaptchaPredicate m_pred;
};

#endif // CAPTCHAAWAREPAGE_H
