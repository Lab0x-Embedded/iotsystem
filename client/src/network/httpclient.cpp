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

void HttpClient::setUserId(int id) {
    if (m_userId != id) {
        m_userId = id;
        emit userIdChanged();
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

// ==================== 用户认证 ====================

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
            int uid = obj["user_id"].toInt(0);
            if (!token.isEmpty()) {
                setAuthToken(token);
                setUserId(uid);
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

// ==================== 设备管理 ====================

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

void HttpClient::updateDevice(const QString &deviceId,
                               const QString &name,
                               int groupId) {
    QJsonObject body;
    body["action"] = "update";
    body["device_id"] = deviceId;
    if (!name.isEmpty()) body["name"] = name;
    if (groupId >= 0)    body["group_id"] = groupId;

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this, deviceId, groupId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit deviceUpdated(deviceId, groupId);
        },
        [this](const QString &error) {
            emit deviceOperationError(error);
        }
    );
}

// ==================== 分组管理 ====================

void HttpClient::fetchGroups() {
    QJsonObject body;
    body["action"] = "query_all";

    auto *reply = m_mgr.post(makeRequest("/api/group"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            QJsonArray groups;
            if (obj.contains("data")) {
                groups = obj["data"].toArray();
            }
            emit groupsFetched(groups);
        },
        [this](const QString &error) {
            emit groupOperationError(error);
        }
    );
}

void HttpClient::createGroup(const QString &name, int parentId, const QString &description) {
    QJsonObject body;
    body["action"] = "create";
    body["name"] = name;
    body["parent_id"] = parentId;
    body["description"] = description;

    auto *reply = m_mgr.post(makeRequest("/api/group"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this](const QJsonObject &obj) {
            emit groupCreated(obj["group_id"].toInt());
        },
        [this](const QString &error) {
            emit groupOperationError(error);
        }
    );
}

void HttpClient::updateGroup(int groupId, const QString &name, const QString &description) {
    QJsonObject body;
    body["action"] = "update";
    body["group_id"] = groupId;
    body["name"] = name;
    body["description"] = description;

    auto *reply = m_mgr.post(makeRequest("/api/group"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this, groupId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit groupUpdated(groupId);
        },
        [this](const QString &error) {
            emit groupOperationError(error);
        }
    );
}

void HttpClient::deleteGroup(int groupId) {
    QJsonObject body;
    body["action"] = "delete";
    body["group_id"] = groupId;

    auto *reply = m_mgr.post(makeRequest("/api/group"),
                             QJsonDocument(body).toJson());
    
    handleReply(reply,
        [this, groupId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit groupDeleted(groupId);
        },
        [this](const QString &error) {
            emit groupOperationError(error);
        }
    );
}

// ==================== 设备影子 ====================

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

// ==================== 指令下发 ====================

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

// ==================== 告警 ====================

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

void HttpClient::addAlarmRule(const QString &deviceId, const QString &metric, int op, double threshold, int severity) {
    QJsonObject body;
    body["action"] = "add_rule";
    body["device_id"] = deviceId;
    body["metric"] = metric;
    body["op"] = op;
    body["threshold"] = threshold;
    body["severity"] = severity;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit alarmRuleAdded();
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}
void HttpClient::fetchDevicesByGroup(int groupId) {
    QJsonObject body;
    body["action"] = "query_by_group";
    body["group_id"] = groupId;

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this, groupId](const QJsonObject &obj) {
            QJsonArray devices;
            if (obj.contains("data")) {
                devices = obj["data"].toArray();
            }
            emit groupDevicesFetched(groupId, devices);
        },
        [this](const QString &error) {
            emit deviceOperationError(error);
        }
    );
}

void HttpClient::queryRules() {
    QJsonObject body;
    body["action"] = "query_rules";

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            QJsonArray rules;
            if (obj.contains("data")) {
                rules = obj["data"].toArray();
            }
            emit rulesFetched(rules);
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::toggleRule(ulong ruleId, bool enabled) {
    QJsonObject body;
    body["action"] = "toggle_rule";
    body["id"] = (double)ruleId;
    body["enabled"] = enabled ? 1 : 0;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit ruleToggled();
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::editRule(ulong ruleId, const QString &deviceId, const QString &metric, int op, double threshold, int severity) {
    QJsonObject body;
    body["action"] = "edit_rule";
    body["id"] = (double)ruleId;
    body["device_id"] = deviceId;
    body["metric"] = metric;
    body["op"] = op;
    body["threshold"] = threshold;
    body["severity"] = severity;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit ruleEdited();
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::deleteRule(ulong ruleId) {
    QJsonObject body;
    body["action"] = "delete_rule";
    body["id"] = (double)ruleId;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit ruleDeleted();
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::acknowledgeAlarm(ulong alarmId, int userId) {
    QJsonObject body;
    body["action"] = "acknowledge";
    body["id"] = (double)alarmId;
    body["user_id"] = (userId > 0) ? userId : m_userId;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this, alarmId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit alarmAcknowledged(alarmId);
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::resolveAlarm(ulong alarmId) {
    QJsonObject body;
    body["action"] = "resolve";
    body["id"] = (double)alarmId;
    body["user_id"] = m_userId;

    auto *reply = m_mgr.post(makeRequest("/api/alarm"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this, alarmId](const QJsonObject &obj) {
            Q_UNUSED(obj);
            emit alarmResolved(alarmId);
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}

void HttpClient::fetchDataPointHistory(const QString &deviceId, const QString &metric,
                                        quint64 startTs, quint64 endTs, int limit) {
    QJsonObject body;
    body["action"] = "query_history";
    body["device_id"] = deviceId;
    body["metric"] = metric;
    body["start_ts"] = (double)startTs;
    body["end_ts"] = (double)endTs;
    body["limit"] = limit;

    auto *reply = m_mgr.post(makeRequest("/api/device"),
                             QJsonDocument(body).toJson());

    handleReply(reply,
        [this](const QJsonObject &obj) {
            QJsonArray points = obj["data"].toArray();
            emit dataPointHistoryFetched(points);
        },
        [this](const QString &error) {
            emit alarmError(error);
        }
    );
}
