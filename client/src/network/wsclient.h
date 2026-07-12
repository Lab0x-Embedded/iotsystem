#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class WsClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)

public:
    explicit WsClient(QObject *parent = nullptr);

    bool isConnected() const { return m_connected; }
    QString serverUrl() const { return m_url; }

    Q_INVOKABLE void connectToServer(const QString &url);
    Q_INVOKABLE void disconnect();

signals:
    void connectedChanged();
    void serverUrlChanged();
    void datapointReceived(const QString &deviceId, const QString &metric, double value, quint64 ts);
    void alarmReceived(const QString &deviceId, const QString &metric, double value, double threshold, int severity);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void tryReconnect();

private:
    void sendHttpRequest();

    QTcpSocket m_socket;
    QTimer m_reconnectTimer;
    bool m_connected = false;
    bool m_shouldConnect = false;
    QString m_url;
    QString m_host;
    quint16 m_port = 8080;
    QByteArray m_buffer;
};

#endif // WSCLIENT_H
