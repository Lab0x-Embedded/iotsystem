#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    void setAuthToken(const QString &token);
    void login(const QString &username, const QString &password);
    void fetchDevices();
    void sendCommand(const QString &deviceId, const QString &cmd, const QJsonObject &payload);

signals:
    void loginSucceeded(const QString &token, const QString &role);
    void loginFailed(const QString &error);
    void devicesFetched(const QJsonDocument &doc);
    void commandSent(bool ok, const QString &status, const QString &commandId);
    void requestError(const QString &error);

private:
    QNetworkAccessManager m_mgr;
    QString m_baseUrl;
    QNetworkRequest makeRequest(const QString &path);
};
#endif
