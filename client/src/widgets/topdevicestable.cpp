#include "topdevicestable.h"
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QtAlgorithms>

TopDevicesTable::TopDevicesTable(QWidget *parent)
    : QWidget(parent), m_grid(new QGridLayout(this)) {
    m_grid->setSpacing(4);
}

static bool greaterTemp(const DeviceInfo &a, const DeviceInfo &b) {
    return a.temperature > b.temperature;
}

void TopDevicesTable::updateTopDevices(const QVector<DeviceInfo> &devices, int column) {
    QLayoutItem *item;
    while ((item = m_grid->takeAt(0))) {
        delete item->widget(); delete item;
    }
    m_column = column;
    QVector<DeviceInfo> sorted = devices;
    std::sort(sorted.begin(), sorted.end(), greaterTemp);
    int limit = qMin(5, sorted.size());
    QLabel *h1 = new QLabel("#"); h1->setStyleSheet("color:#a6adc8;font-weight:bold;");
    QLabel *h2 = new QLabel("设备"); h2->setStyleSheet("color:#a6adc8;font-weight:bold;");
    QLabel *h3 = new QLabel("温度"); h3->setStyleSheet("color:#a6adc8;font-weight:bold;");
    QLabel *h4 = new QLabel("湿度"); h4->setStyleSheet("color:#a6adc8;font-weight:bold;");
    QLabel *h5 = new QLabel("电量"); h5->setStyleSheet("color:#a6adc8;font-weight:bold;");
    m_grid->addWidget(h1, 0, 0); m_grid->addWidget(h2, 0, 1);
    m_grid->addWidget(h3, 0, 2); m_grid->addWidget(h4, 0, 3); m_grid->addWidget(h5, 0, 4);
    for (int i = 0; i < limit; ++i) {
        const auto &d = sorted[i];
        int row = i + 1;
        m_grid->addWidget(new QLabel(QString::number(i + 1)), row, 0);
        m_grid->addWidget(new QLabel(d.id), row, 1);
        m_grid->addWidget(new QLabel(QString::number(d.temperature, 'f', 1) + "°C"), row, 2);
        m_grid->addWidget(new QLabel(QString::number(d.humidity, 'f', 0) + "%"), row, 3);
        m_grid->addWidget(new QLabel(QString::number(d.battery, 'f', 0) + "%"), row, 4);
    }
}
