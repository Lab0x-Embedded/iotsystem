#include "httpclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

HttpClient::HttpClient(QObject *parent) : QObject(parent) {}

void HttpClient::setServerUrl(const QString &url) {
    if (m_baseUrl != url) {
        m_baseUrl = url;
        emit serverUrlChanged();
        m_connected = !url.isEmpty();
        emit connectedChanged();
    }
}

void HttpClient::setAuthToken(const QString &token) {
    if (m_token != token) {
        m_token = token;
        emit authTokenChanged();
    }
}

QNetworkRequest HttpClient::makeRequest(const QString &path) {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    if (!m_token.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    }
    return req;
}

void HttpClient::handleReply(QNetworkReply *reply, 
                              std::function<void(const QJsonObject &)> onSuccess,
                              std::function<void(const QString &)> onError) {
    connect(reply, &QNetworkReply::finished, this, [reply, onSuccess, onError]() {
        QByteArray data = reply->readAll();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString error = reply->errorString();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject() && doc.object().contains("error")) {
                error = doc.object()["error"].toString();
            }
            if (onError) onError(error);
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                if (onSuccess) onSuccess(doc.object());
            } else if (doc.isArray()) {
                QJsonObject wrapper;
                wrapper["data"] = doc.array();
                if (onSuccess) onSuccess(wrapper);
            }
        }
        reply->deleteLater();
    });
}

void HttpClient::login(const QString &username, const QString &password) {
    QJsonObject body;
    body["action"] = "login";
    body["username"] = username;
    body["password"] = password;

    auto *reply = m_mgr.post(makeRequest("/api/user"), 
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            QString token = obj["token"].toString();
            QString role = obj["role"].toString();
            if (!token.isEmpty()) {
                setAuthToken(token);
                m_connected = true;
                emit connectedChanged();
                emit loginSucceeded(token, role);
            } else {
                emit loginFailed("Invalid response");
            }
        },
        [this](const QString &error) {
            emit loginFailed(error);
        }
    );
}

void HttpClient::fetchDevices() {
    QJsonObject body;
    body["action"] = "query_all";

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            QJsonArray devices;
            if (obj.contains("data")) {
                devices = obj["data"].toArray();
            }
            emit devicesFetched(devices);
        },
        [this](const QString &error) {
            emit deviceOperationError(error);
        }
    );
}

void HttpClient::registerDevice(const QString &deviceId, const QString &name,
                                 const QString &productKey, const QString &groupId) {
    QJsonObject body;
    body["action"] = "register";
    body["device_id"] = deviceId;
    body["name"] = name;
    body["product_key"] = productKey;
    body["group_id"] = groupId;

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this, deviceId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit deviceRegistered(deviceId);
        },
        [this](const QString &error) {
            emit deviceOperationError(error);
        }
    );
}

void HttpClient::queryDevice(const QString &deviceId) {
    QJsonObject body;
    body["action"] = "query";
    body["device_id"] = deviceId;

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            emit deviceQueryResult(obj);
        },
        [this](const QString &error) {
            emit deviceOperationError(error);
        }
    );
}

void HttpClient::getShadow(const QString &deviceId) {
    QJsonObject body;
    body["action"] = "get";
    body["device_id"] = deviceId;

    auto *reply = m_mgr.post(makeRequest("/api/shadow"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this, deviceId](const QJsonObject &obj) {
            emit shadowFetched(deviceId, obj);
        },
        [this](const QString &error) {
            emit shadowError(error);
        }
    );
}

void HttpClient::updateShadow(const QString &deviceId, const QJsonObject &desired) {
    QJsonObject body;
    body["action"] = "update";
    body["device_id"] = deviceId;
    body["desired"] = desired;

    auto *reply = m_mgr.post(makeRequest("/api/shadow"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this, deviceId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit shadowUpdated(deviceId);
        },
        [this](const QString &error) {
            emit shadowError(error);
        }
    );
}

void HttpClient::sendCommand(const QString &deviceId, const QString &cmd,
                              const QJsonObject &payload) {
    QJsonObject body;
    body["action"] = "send";
    body["device_id"] = deviceId;
    body["cmd"] = cmd;
    body["payload"] = payload;

    auto *reply = m_mgr.post(makeRequest("/api/command"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            bool ok = !obj.contains("error");
            emit commandSent(ok, obj["status"].toString(), obj["command_id"].toString());
        },
        [this](const QString &error) {
            emit commandError(error);
        }
    );
}

void HttpClient::fetchAlarms(const QString &deviceId) {
    QJsonObject body;
    body["action"] = "query";
    if (!deviceId.isEmpty()) {
        body["device_id"] = deviceId;
    }

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            QJsonArray alarms;
            if (obj.contains("data")) {
                alarms = obj["data"].toArray();
            }
            emit alarmsFetched(alarms);
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}
