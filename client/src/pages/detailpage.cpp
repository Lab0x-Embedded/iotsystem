#include "detailpage.h"
#include "widgets/realtimechart.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTime>
#include <QScrollArea>
#include "theme/theme.h"

/* ============================================
 * DetailPage - Device Detail View
 * Following Qt UI Design Guidelines:
 * - Progressive Disclosure: Show relevant data
 * - Modularity: Self-contained sections
 * - F-shaped reading: Text-heavy content layout
 * - Recognition Over Recall: Show options
 * ============================================ */

DetailPage::DetailPage(QWidget *parent) : QWidget(parent) {
    QScrollArea *scroll;
    QWidget *host = new QWidget;
    auto *root = new QVBoxLayout(host);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // Title row
    auto *titleRow = new QHBoxLayout;
    m_lblName = new QLabel("未选择设备");
    m_lblName->setStyleSheet("font-size:18px; font-weight:bold;");
    
    m_lblStatus = new QLabel("-");
    m_lblStatus->setStyleSheet("font-size:14px;");
    
    titleRow->addWidget(m_lblName);
    titleRow->addStretch();
    titleRow->addWidget(m_lblStatus);
    root->addLayout(titleRow);

    setupShadowPanel(root);
    setupChartPanel(root);
    setupControlPanel(root);
    setupHistoryPanel(root);

    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(host);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

void DetailPage::setupShadowPanel(QVBoxLayout *root) {
    QGroupBox *box = new QGroupBox("Device Shadow");
    auto *grid = new QGridLayout(box);
    grid->setSpacing(12);
    grid->setContentsMargins(8, 16, 8, 8);
    
    auto *desiredLabel = new QLabel("Desired (期望状态):");
    desiredLabel->setStyleSheet("font-weight:bold;");
    auto *reportedLabel = new QLabel("Reported (上报状态):");
    reportedLabel->setStyleSheet("font-weight:bold;");
    
    grid->addWidget(desiredLabel, 0, 0);
    grid->addWidget(reportedLabel, 0, 1);
    
    m_desiredEdit = new QTextEdit;
    m_desiredEdit->setPlaceholderText("{\"temperature\":25,\"fan_speed\":\"high\"}");
    m_desiredEdit->setMinimumHeight(100);
    m_desiredEdit->setMaximumHeight(150);
    
    m_reportedEdit = new QTextEdit;
    m_reportedEdit->setReadOnly(true);
    m_reportedEdit->setMinimumHeight(100);
    m_reportedEdit->setMaximumHeight(150);
    
    grid->addWidget(m_desiredEdit, 1, 0);
    grid->addWidget(m_reportedEdit, 1, 1);
    
    QPushButton *applyBtn = new QPushButton("修改期望值");
    applyBtn->setObjectName("primaryButton");
    applyBtn->setMinimumHeight(36);
    grid->addWidget(applyBtn, 2, 0, 1, 2, Qt::AlignLeft);
    
    root->addWidget(box);
}

void DetailPage::setupChartPanel(QVBoxLayout *root) {
    QGroupBox *box = new QGroupBox("实时数据折线图");
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(8, 16, 8, 8);
    
    m_chartTemp = new RealtimeChart("温度/湿度");
    m_chartTemp->addSeries("温度", QColor("#f38ba8"));
    m_chartTemp->addSeries("湿度", QColor("#89b4fa"));
    m_chartTemp->setMinimumHeight(280);
    lay->addWidget(m_chartTemp);
    
    root->addWidget(box);
}

void DetailPage::setupControlPanel(QVBoxLayout *root) {
    QGroupBox *box = new QGroupBox("远程控制");
    auto *grid = new QGridLayout(box);
    grid->setSpacing(8);
    grid->setContentsMargins(8, 16, 8, 8);
    
    QStringList cmds = {"开启风扇", "关闭风扇", "设置温度", "重启设备"};
    int i = 0;
    for (const auto &c : cmds) {
        auto *btn = new QPushButton(c);
        btn->setMinimumHeight(36);
        grid->addWidget(btn, i / 2, i % 2);
        ++i;
    }
    
    auto *cmdLabel = new QLabel("自定义指令:");
    cmdLabel->setStyleSheet("font-weight:bold;");
    grid->addWidget(cmdLabel, 2, 0);
    
    m_cmdInput = new QLineEdit;
    m_cmdInput->setPlaceholderText("例如: reboot");
    m_cmdInput->setMinimumHeight(36);
    grid->addWidget(m_cmdInput, 2, 1);
    
    auto *send = new QPushButton("发送");
    send->setObjectName("primaryButton");
    send->setMinimumHeight(36);
    grid->addWidget(send, 2, 2);
    
    root->addWidget(box);
}

void DetailPage::setupHistoryPanel(QVBoxLayout *root) {
    QGroupBox *box = new QGroupBox("历史数据查询");
    auto *row = new QHBoxLayout(box);
    row->setSpacing(8);
    row->setContentsMargins(8, 16, 8, 8);
    
    QComboBox *range = new QComboBox;
    range->addItems({"最近24小时", "最近7天", "最近30天"});
    range->setMinimumHeight(36);
    
    QComboBox *metric = new QComboBox;
    metric->addItems({"温度", "湿度", "电量"});
    metric->setMinimumHeight(36);
    
    QPushButton *query = new QPushButton("查询");
    query->setMinimumHeight(36);
    
    QPushButton *exportCsv = new QPushButton("导出CSV");
    exportCsv->setMinimumHeight(36);
    
    auto *rangeLabel = new QLabel("时间范围:");
    rangeLabel->setStyleSheet("font-weight:bold;");
    auto *metricLabel = new QLabel("指标:");
    metricLabel->setStyleSheet("font-weight:bold;");
    
    row->addWidget(rangeLabel);
    row->addWidget(range);
    row->addWidget(metricLabel);
    row->addWidget(metric);
    row->addWidget(query);
    row->addWidget(exportCsv);
    row->addStretch();
    
    root->addWidget(box);
}

void DetailPage::addDataPoint(const QString &metric, double value, qint64 timestamp) {
    if (!m_chartTemp) return;
    if (metric == "temperature" || metric == "humidity") {
        m_chartTemp->appendPoint(metric == "temperature" ? "温度" : "湿度",
                                 QPointF(timestamp, value));
    }
}

void DetailPage::showDevice(const DeviceInfo &device) {
    m_current = device;
    m_lblName->setText(QString("%1  (%2)").arg(device.name, device.id));
    m_lblStatus->setText(QString("状态: %1  ·  最后上报: %2")
                             .arg(device.statusText(),
                                  device.lastSeen.isValid() ? device.lastSeen.toString("yyyy-MM-dd hh:mm:ss") : "-"));
    m_lblStatus->setStyleSheet(QString("color:%1; font-size:14px;").arg(device.statusColor().name()));
    
    m_reportedEdit->setPlainText(QString("{\n  \"temperature\": %1,\n  \"humidity\": %2,\n  \"battery\": %3,\n  \"online\": %4\n}")
                                     .arg(device.temperature, 0, 'f', 1)
                                     .arg(device.humidity, 0, 'f', 1)
                                     .arg(device.battery, 0, 'f', 0)
                                     .arg(device.status == DeviceStatus::Online ? "true" : "false"));
}
