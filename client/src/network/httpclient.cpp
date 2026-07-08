#include "httpclient.h"
#include <QJsonDocument>
#include <QJsonObject>

HttpClient::HttpClient(QObject *parent) : QObject(parent) {}

void HttpClient::setBaseUrl(const QString &url) { m_baseUrl = url; }
void HttpClient::setAuthToken(const QString &token) { Q_UNUSED(token); /* stored by caller */ }

QNetworkRequest HttpClient::makeRequest(const QString &path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    return req;
}

void HttpClient::login(const QString &username, const QString &password) {
    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    auto *reply = m_mgr.post(makeRequest("/api/user"),
                             QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply, this]() {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        if (root.contains("token")) {
            emit loginSucceeded(root["token"].toString(), root["role"].toString());
        } else {
            emit loginFailed(root["error"].toString());
        }
        reply->deleteLater();
    });
}

void HttpClient::fetchDevices() {
    auto *reply = m_mgr.get(makeRequest("/api/device"));
    connect(reply, &QNetworkReply::finished, this, [reply, this]() {
        emit devicesFetched(QJsonDocument::fromJson(reply->readAll()));
        reply->deleteLater();
    });
}

void HttpClient::sendCommand(const QString &deviceId, const QString &cmd, const QJsonObject &payload) {
    QJsonObject body;
    body["device_id"] = deviceId;
    body["cmd"] = cmd;
    body["payload"] = payload;
    auto *reply = m_mgr.post(makeRequest("/api/command"),
                             QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply, this]() {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        bool ok = !root.contains("error");
        emit commandSent(ok, root["status"].toString(), root["command_id"].toString());
        reply->deleteLater();
    });
}
