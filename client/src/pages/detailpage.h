#ifndef DETAILPAGE_H
#define DETAILPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QFrame>
#include "models/devicemodel.h"

class RealtimeChart;

class DetailPage : public QWidget {
    Q_OBJECT
public:
    explicit DetailPage(QWidget *parent = nullptr);

public slots:
    void showDevice(const DeviceInfo &device);
    void addDataPoint(const QString &metric, double value, qint64 timestamp);

private:
    DeviceInfo m_current;

    // === Device Status Card ===
    QFrame *m_statusCard;
    QLabel *m_lblName;
    QLabel *m_lblStatusDot;
    QLabel *m_lblStatusText;
    QLabel *m_lblTemperature;
    QLabel *m_lblHumidity;
    QLabel *m_lblLastUpdate;

    // === Shadow Panel ===
    QPlainTextEdit *m_desiredEdit;
    QPlainTextEdit *m_reportedEdit;

    // === Chart Panel ===
    RealtimeChart *m_chartTemp;
    QLabel *m_lblChartTemp;
    QLabel *m_lblChartHumidity;
    QLabel *m_lblChartStatus;

    // === Control Panel ===
    QLineEdit *m_cmdInput;

    void setupStatusCard(QVBoxLayout *root);
    void setupShadowPanel(QVBoxLayout *root);
    void setupChartPanel(QVBoxLayout *root);
    void setupControlPanel(QVBoxLayout *root);
    void setupHistoryPanel(QVBoxLayout *root);
};
#endif
