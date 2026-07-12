#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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
    void onReadyRead();
    void onDisconnected();

private:
    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    bool m_connected = false;
    QString m_url;
    QByteArray m_buffer;
};

#endif // WSCLIENT_H
