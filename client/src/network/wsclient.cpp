#include "wsclient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

WsClient::WsClient(QObject *parent) : QObject(parent) {
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(3000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &WsClient::tryReconnect);

    connect(&m_socket, &QTcpSocket::connected, this, &WsClient::onConnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &WsClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &WsClient::onDisconnected);
}

void WsClient::connectToServer(const QString &url) {
    m_url = url;
    m_shouldConnect = true;
    emit serverUrlChanged();

    QUrl qurl(url);
    m_host = qurl.host();
    m_port = qurl.port(8080);

    tryReconnect();
}

void WsClient::disconnect() {
    m_shouldConnect = false;
    m_reconnectTimer.stop();
    m_socket.abort();
    m_connected = false;
    emit connectedChanged();
}

void WsClient::tryReconnect() {
    if (!m_shouldConnect || m_host.isEmpty()) return;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }
    /* SSE 和 HTTP 同端口 */
    qDebug() << "[WsClient] connecting to SSE" << m_host << ":" << m_port;
    m_socket.connectToHost(m_host, m_port);
}

void WsClient::onConnected() {
    qDebug() << "[WsClient] TCP connected, sending SSE request";
    sendHttpRequest();
}

void WsClient::sendHttpRequest() {
    QString request = QString("GET /api/sse HTTP/1.1\r\n"
                              "Host: %1:%2\r\n"
                              "Accept: text/event-stream\r\n"
                              "Cache-Control: no-cache\r\n"
                              "Connection: keep-alive\r\n\r\n")
                              .arg(m_host).arg(m_port);
    m_socket.write(request.toUtf8());
    m_socket.flush();
}

void WsClient::onReadyRead() {
    m_buffer.append(m_socket.readAll());

    /* 解析 SSE 格式: "data: {...}\n\n" */
    while (true) {
        int idx = m_buffer.indexOf("\n\n");
        if (idx < 0) break;

        QByteArray event = m_buffer.left(idx);
        m_buffer = m_buffer.mid(idx + 2);

        /* 跳过 HTTP 头和注释 */
        if (event.startsWith("HTTP/") || event.startsWith(":")) {
            if (!m_connected && !event.startsWith(":")) {
                m_connected = true;
                emit connectedChanged();
            }
            continue;
        }

        /* 标记已连接 */
        if (!m_connected) {
            m_connected = true;
            emit connectedChanged();
        }

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
    bool wasConnected = m_connected;
    m_connected = false;
    m_buffer.clear();
    emit connectedChanged();

    if (wasConnected) {
        qDebug() << "[WsClient] SSE disconnected, will reconnect in 3s";
    }

    if (m_shouldConnect) {
        m_reconnectTimer.start();
    }
}
