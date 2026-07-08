#include "mainwindow.h"
#include "navsidebar.h"
#include "datamanager.h"
#include "pages/overviewpage.h"
#include "pages/detailpage.h"
#include "pages/dashboardpage.h"
#include "pages/alarmcenterpage.h"
#include <QHBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("IoT Device Manager");
    resize(1280, 800);
    setObjectName("MainWindow");

    m_data = new DataManager(this);
    m_sidebar = new NavSidebar(this);

    m_overview = new OverviewPage(m_data->deviceModel(), this);
    m_detail = new DetailPage(this);
    m_dashboard = new DashboardPage(this);
    m_alarm = new AlarmCenterPage(m_data->alarmModel(), this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_overview);
    m_stack->addWidget(m_detail);
    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_alarm);

    auto *center = new QWidget(this);
    auto *lay = new QHBoxLayout(center);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_sidebar);
    lay->addWidget(m_stack, 1);
    setCentralWidget(center);

    auto *status = new QLabel("数据源: Mock (离线演示模式)  ·  共 0 设备");
    status->setStyleSheet("color:#a6adc8; padding:4px 12px;");
    statusBar()->addPermanentWidget(status);
    statusBar()->setStyleSheet("background:#11111b; border-top:1px solid #313244;");

    connect(m_sidebar, &NavSidebar::pageSelected, this, &MainWindow::switchPage);
    connect(m_overview, &OverviewPage::deviceSelected, this, &MainWindow::showDeviceDetail);
    connect(m_data, &DataManager::countsChanged, status, [status](int tot, int on, int al) {
        status->setText(QString("数据源: Mock (离线演示模式)  ·  共 %1 设备  ·  在线 %2  ·  告警 %3").arg(tot).arg(on).arg(al));
    });
    connect(m_data, &DataManager::deviceUpdated, m_data->deviceModel(), &DeviceModel::updateDevice);
    connect(m_data, &DataManager::newAlarm, m_data->alarmModel(), &AlarmModel::addRecord);
    connect(m_data, &DataManager::deviceUpdated, m_dashboard, [this](const DeviceInfo &d) {
        Q_UNUSED(d);
        QVector<DeviceInfo> all;
        for (int i = 0; i < m_data->deviceModel()->rowCount(); ++i)
            all.push_back(m_data->deviceModel()->deviceAt(i));
        m_dashboard->setDevices(all);
    });
    connect(m_data, &DataManager::dataPointArrived, m_dashboard, [this](const QString &devId, const QString &metric, double v, qint64 ts) {
        m_dashboard->addDataPoint(metric, v, ts);
    });
    connect(m_data, &DataManager::dataPointArrived, m_detail, [this](const QString &devId, const QString &metric, double v, qint64 ts) {
        m_detail->addDataPoint(metric, v, ts);
    });
    connect(m_data, &DataManager::newAlarm, status, [status]() {
        /* alarm count updates via overview */
    });

    m_data->start();
}

MainWindow::~MainWindow() { m_data->stop(); }

void MainWindow::switchPage(int index) {
    if (index >= 0 && index < m_stack->count()) m_stack->setCurrentIndex(index);
}

void MainWindow::showDeviceDetail(const QString &deviceId) {
    for (int i = 0; i < m_data->deviceModel()->rowCount(); ++i) {
        const auto &d = m_data->deviceModel()->deviceAt(i);
        if (d.id == deviceId) {
            m_detail->showDevice(d);
            m_stack->setCurrentIndex(1);
            m_sidebar->findChild<QButtonGroup *>()->button(1)->setChecked(true);
            return;
        }
    }
}
