#include "wsclient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

WsClient::WsClient(QObject *parent) : QObject(parent) {
}

void WsClient::connectToServer(const QString &url) {
    if (m_reply) {
        disconnect();
    }
    m_url = url;
    emit serverUrlChanged();

    /* SSE 端点和 HTTP API 同端口 */
    QString sseUrl = url + "/api/sse";
    qDebug() << "[WsClient] connecting to SSE:" << sseUrl;

    QNetworkRequest request;
    request.setUrl(QUrl(sseUrl));
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");

    m_reply = m_nam.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &WsClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &WsClient::onDisconnected);

    m_connected = true;
    emit connectedChanged();
}

void WsClient::disconnect() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_connected = false;
    emit connectedChanged();
}

void WsClient::onReadyRead() {
    if (!m_reply) return;
    m_buffer.append(m_reply->readAll());

    /* 解析 SSE 格式: "data: {...}\n\n" */
    while (true) {
        int idx = m_buffer.indexOf("\n\n");
        if (idx < 0) break;

        QByteArray event = m_buffer.left(idx);
        m_buffer = m_buffer.mid(idx + 2);

        /* 提取 data 行 */
        QList<QByteArray> lines = event.split('\n');
        for (const QByteArray &line : lines) {
            if (line.startsWith("data: ")) {
                QByteArray jsonStr = line.mid(6);
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr);
                if (!doc.isObject()) continue;
                QJsonObject obj = doc.object();
                QString type = obj["type"].toString();

                if (type == "datapoint") {
                    emit datapointReceived(
                        obj["device_id"].toString(),
                        obj["metric"].toString(),
                        obj["value"].toDouble(),
                        (quint64)obj["ts"].toDouble()
                    );
                } else if (type == "alarm") {
                    emit alarmReceived(
                        obj["device_id"].toString(),
                        obj["metric"].toString(),
                        obj["value"].toDouble(),
                        obj["threshold"].toDouble(),
                        obj["severity"].toInt()
                    );
                }
            }
        }
    }
}

void WsClient::onDisconnected() {
    qDebug() << "[WsClient] SSE disconnected";
    m_connected = false;
    m_reply = nullptr;
    emit connectedChanged();
}
