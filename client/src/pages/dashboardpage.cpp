#include "dashboardpage.h"
#include "widgets/gaugewidget.h"
#include "widgets/realtimechart.h"
#include "widgets/topdevicestable.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QtAlgorithms>
#include "theme/theme.h"

/* ============================================
 * DashboardPage - Data Visualization Dashboard
 * Following Qt UI Design Guidelines:
 * - Golden Ratio + Rule of Thirds: Layout structure
 * - Progressive Disclosure: Show relevant data
 * - Modular: Self-contained sections
 * - Layer-cake: Clear section headers
 * ============================================ */

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // Title - Primary typography level
    m_lblTotal = new QLabel("IoT 设备管理数据大屏");
    m_lblTotal->setStyleSheet("font-size:20px; font-weight:bold;");
    root->addWidget(m_lblTotal);

    // Top row - Gauge widgets
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    
    auto *gaugeGroup = new QGroupBox("关键指标仪表盘");
    auto *gaugeLay = new QHBoxLayout(gaugeGroup);
    gaugeLay->setSpacing(12);
    
    m_gaugeTemp = new GaugeWidget("平均温度");
    m_gaugeTemp->setRange(0, 50);
    m_gaugeTemp->setUnit("°C");
    m_gaugeTemp->setColor(QColor("#f38ba8"));
    
    m_gaugeHumid = new GaugeWidget("平均湿度");
    m_gaugeHumid->setRange(0, 100);
    m_gaugeHumid->setUnit("%");
    m_gaugeHumid->setColor(QColor("#89b4fa"));
    
    m_gaugeBattery = new GaugeWidget("平均电量");
    m_gaugeBattery->setRange(0, 100);
    m_gaugeBattery->setUnit("%");
    m_gaugeBattery->setColor(QColor("#a6e3a1"));
    
    m_gaugeOnline = new GaugeWidget("在线率");
    m_gaugeOnline->setRange(0, 100);
    m_gaugeOnline->setUnit("%");
    m_gaugeOnline->setColor(QColor("#f9e2af"));
    
    gaugeLay->addWidget(m_gaugeTemp);
    gaugeLay->addWidget(m_gaugeHumid);
    gaugeLay->addWidget(m_gaugeBattery);
    gaugeLay->addWidget(m_gaugeOnline);
    topRow->addWidget(gaugeGroup, 2);
    root->addLayout(topRow);

    // Middle row - Chart and Top devices
    auto *midRow = new QHBoxLayout;
    midRow->setSpacing(12);
    
    auto *chartBox = new QGroupBox("实时滚动数据曲线");
    auto *chartLay = new QVBoxLayout(chartBox);
    chartLay->setContentsMargins(8, 16, 8, 8);
    
    m_chart = new RealtimeChart("温度/湿度");
    m_chart->addSeries("温度", QColor("#f38ba8"));
    m_chart->addSeries("湿度", QColor("#89b4fa"));
    m_chart->setMinimumHeight(280);
    chartLay->addWidget(m_chart);
    midRow->addWidget(chartBox, 2);

    auto *topBox = new QGroupBox("TOP5 设备数据排行");
    auto *topLay = new QVBoxLayout(topBox);
    topLay->setContentsMargins(8, 16, 8, 8);
    
    m_topTable = new TopDevicesTable;
    topLay->addWidget(m_topTable);
    midRow->addWidget(topBox, 1);
    root->addLayout(midRow);
    root->setStretch(2, 1);
}

void DashboardPage::setDevices(const QVector<DeviceInfo> &devices) {
    m_topTable->updateTopDevices(devices, 0);
    if (devices.isEmpty()) return;
    
    double sumT = 0, sumH = 0, sumB = 0;
    int on = 0;
    
    for (const auto &d : devices) {
        sumT += d.temperature;
        sumH += d.humidity;
        sumB += d.battery;
        if (d.status == DeviceStatus::Online) ++on;
    }
    
    int n = devices.size();
    m_gaugeTemp->setValue(sumT / n);
    m_gaugeHumid->setValue(sumH / n);
    m_gaugeBattery->setValue(sumB / n);
    m_gaugeOnline->setValue(100.0 * on / n);
}

void DashboardPage::addDataPoint(const QString &metric, double value, qint64 timestamp) {
    if (metric == "temperature") {
        m_chart->appendPoint("温度", QPointF(timestamp, value));
    } else if (metric == "humidity") {
        m_chart->appendPoint("湿度", QPointF(timestamp, value));
    }
}
