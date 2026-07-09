#include "topdevicestable.h"
#include "theme/theme.h"
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QtAlgorithms>
#include <QFrame>

/* ============================================
 * TopDevicesTable - Top Devices Ranking
 * Following Qt UI Design Guidelines:
 * - F-shaped reading: Clear column headers
 * - Recognition Over Recall: Visual hierarchy
 * - Uniform Connectedness: Visual grouping
 * ============================================ */

TopDevicesTable::TopDevicesTable(QWidget *parent)
    : QWidget(parent), m_grid(new QGridLayout(this)) {
    m_grid->setSpacing(8);
    m_grid->setContentsMargins(4, 4, 4, 4);
}

static bool greaterTemp(const DeviceInfo &a, const DeviceInfo &b) {
    return a.temperature > b.temperature;
}

void TopDevicesTable::updateTopDevices(const QVector<DeviceInfo> &devices, int column) {
    // Clear existing items
    QLayoutItem *item;
    while ((item = m_grid->takeAt(0))) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    m_column = column;
    QVector<DeviceInfo> sorted = devices;
    std::sort(sorted.begin(), sorted.end(), greaterTemp);
    int limit = qMin(5, sorted.size());
    
    // Header row - Secondary typography level
    auto *h1 = new QLabel("#");
    h1->setStyleSheet("font-weight:bold; font-size:13px; color:#a6adc8;");
    auto *h2 = new QLabel("设备");
    h2->setStyleSheet("font-weight:bold; font-size:13px; color:#a6adc8;");
    auto *h3 = new QLabel("温度");
    h3->setStyleSheet("font-weight:bold; font-size:13px; color:#a6adc8;");
    auto *h4 = new QLabel("湿度");
    h4->setStyleSheet("font-weight:bold; font-size:13px; color:#a6adc8;");
    auto *h5 = new QLabel("电量");
    h5->setStyleSheet("font-weight:bold; font-size:13px; color:#a6adc8;");
    
    m_grid->addWidget(h1, 0, 0);
    m_grid->addWidget(h2, 0, 1);
    m_grid->addWidget(h3, 0, 2);
    m_grid->addWidget(h4, 0, 3);
    m_grid->addWidget(h5, 0, 4);
    
    // Data rows
    for (int i = 0; i < limit; ++i) {
        const auto &d = sorted[i];
        int row = i + 1;
        
        // Rank number
        auto *rankLabel = new QLabel(QString::number(i + 1));
        rankLabel->setStyleSheet("font-weight:bold; font-size:14px;");
        m_grid->addWidget(rankLabel, row, 0);
        
        // Device ID
        auto *idLabel = new QLabel(d.id);
        idLabel->setStyleSheet("font-size:14px;");
        m_grid->addWidget(idLabel, row, 1);
        
        // Temperature
        auto *tempLabel = new QLabel(QString::number(d.temperature, 'f', 1) + "°C");
        tempLabel->setStyleSheet("font-size:14px;");
        m_grid->addWidget(tempLabel, row, 2);
        
        // Humidity
        auto *humidLabel = new QLabel(QString::number(d.humidity, 'f', 0) + "%");
        humidLabel->setStyleSheet("font-size:14px;");
        m_grid->addWidget(humidLabel, row, 3);
        
        // Battery
        auto *batteryLabel = new QLabel(QString::number(d.battery, 'f', 0) + "%");
        batteryLabel->setStyleSheet("font-size:14px;");
        m_grid->addWidget(batteryLabel, row, 4);
    }
    
    // Add stretch to push content to top
    m_grid->setRowStretch(limit + 1, 1);
}
