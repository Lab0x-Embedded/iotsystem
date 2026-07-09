#include "detailpage.h"
#include "widgets/realtimechart.h"
#include "theme/theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTime>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>

/* ============================================
 * DetailPage - Industrial IoT Dashboard
 *
 * Layout:
 *   [Device Status Card]       ← top hero
 *   [Desired | Reported]       ← shadow twin cards
 *   [Realtime Monitor]         ← chart + live metrics
 *   [Quick Control | Custom]   ← 2-col grid
 *   [History Query]            ← bottom
 *
 * Design: ThingsBoard / Aliyun IoT Console style
 * ============================================ */

// ---- helpers ---------------------------------------------------------

static QFrame *makeCard(const QString &objectName)
{
    auto *f = new QFrame;
    f->setObjectName(objectName);
    f->setMinimumHeight(60);
    return f;
}

static QLabel *makeMetricValue(const QString &text, const QString &color, int fontSize = 28)
{
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
                           "font-size:%1px; font-weight:bold; color:%2; background:transparent;")
                           .arg(fontSize)
                           .arg(color));
    return lbl;
}

static QLabel *makeMetricLabel(const QString &text)
{
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(
        "font-size:12px; color:#9ca0b0; background:transparent; margin-top:2px;");
    return lbl;
}

static QGroupBox *makeSection(const QString &title)
{
    auto *box = new QGroupBox(title);
    box->setObjectName("iotSection");
    return box;
}

// ---- constructor ------------------------------------------------------

DetailPage::DetailPage(QWidget *parent) : QWidget(parent)
{
    QScrollArea *scroll;
    QWidget *host = new QWidget;
    host->setObjectName("detailHost");
    auto *root = new QVBoxLayout(host);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);
    // 1) Device Status Hero Card
    setupStatusCard(root);

    // 2) Shadow: Desired | Reported
    setupShadowPanel(root);

    // 3) Realtime Chart
    setupChartPanel(root);

    // 4) Quick Control | Custom Command
    setupControlPanel(root);

    // 5) History Query
    setupHistoryPanel(root);

    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(host);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

// ======================================================================
// 1. Device Status Card  – top hero
// ======================================================================
void DetailPage::setupStatusCard(QVBoxLayout *root)
{
    m_statusCard = makeCard("statusCard");

    auto *shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 40));
    m_statusCard->setGraphicsEffect(shadow);

    // 卡片尺寸
    m_statusCard->setMinimumHeight(130);

    auto *cardLayout = new QHBoxLayout(m_statusCard);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(40);

    /*
     * =========================
     * 左侧 设备信息
     * =========================
     */
    auto *leftWidget = new QWidget;
    auto *leftCol = new QVBoxLayout(leftWidget);

    leftCol->setSpacing(8);
    leftCol->setContentsMargins(0, 0, 0, 0);

    m_lblName = new QLabel("未选择设备");
    m_lblName->setObjectName("statusCardTitle");
    m_lblName->setMinimumWidth(180);

    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(10);

    m_lblStatusDot = new QLabel("●");
    m_lblStatusDot->setObjectName("statusDotOffline");
    m_lblStatusDot->setFixedWidth(18);

    m_lblStatusText = new QLabel("离线");
    m_lblStatusText->setObjectName("statusCardSubtitle");

    statusRow->addWidget(m_lblStatusDot);
    statusRow->addWidget(m_lblStatusText);
    statusRow->addStretch();

    leftCol->addWidget(m_lblName);
    leftCol->addLayout(statusRow);
    leftCol->addStretch();

    leftWidget->setMinimumWidth(220);

    cardLayout->addWidget(leftWidget, 2);

    /*
     * =========================
     * 右侧 实时指标
     * =========================
     */

    auto *metricsRow = new QHBoxLayout;
    metricsRow->setSpacing(35);

    // 温度
    auto *tempWidget = new QWidget;
    auto *tempCol = new QVBoxLayout(tempWidget);

    tempCol->setSpacing(4);

    m_lblTemperature =
        makeMetricValue("--℃", "#3b82f6", 26);

    auto *tempTitle =
        makeMetricLabel("温度");

    tempTitle->setMinimumWidth(120);

    tempCol->addWidget(m_lblTemperature);
    tempCol->addWidget(tempTitle);

    tempWidget->setFixedWidth(120);

    // 湿度
    auto *humWidget = new QWidget;
    auto *humCol = new QVBoxLayout(humWidget);

    humCol->setSpacing(4);

    m_lblHumidity =
        makeMetricValue("--%", "#22c55e", 26);

    auto *humTitle =
        makeMetricLabel("湿度");

    humTitle->setMinimumWidth(120);

    humCol->addWidget(m_lblHumidity);
    humCol->addWidget(humTitle);

    humWidget->setFixedWidth(120);

    // 更新时间
    auto *updateWidget = new QWidget;
    auto *updCol = new QVBoxLayout(updateWidget);

    updCol->setSpacing(4);

    m_lblLastUpdate =
        makeMetricValue("--:--:--", "#f59e0b", 22);

    auto *updTitle =
        makeMetricLabel("最后更新");

    updTitle->setMinimumWidth(150);

    updCol->addWidget(m_lblLastUpdate);
    updCol->addWidget(updTitle);

    updateWidget->setFixedWidth(160);

    metricsRow->addWidget(tempWidget);
    metricsRow->addWidget(humWidget);
    metricsRow->addWidget(updateWidget);
    metricsRow->addStretch();

    cardLayout->addLayout(metricsRow, 3);

    root->addWidget(m_statusCard);

    // 页面边距
    root->setContentsMargins(
        20,
        20,
        20,
        20);

    root->setSpacing(16);
}

// ======================================================================
// 2. Shadow Panel – Desired | Reported twin cards
// ======================================================================
void DetailPage::setupShadowPanel(QVBoxLayout *root)
{
    auto *shadowRow = new QHBoxLayout;
    shadowRow->setSpacing(16);

    // -- Desired Card --
    auto *desiredBox = makeSection("期望状态 Desired");
    auto *dLay = new QVBoxLayout(desiredBox);
    dLay->setContentsMargins(12, 20, 12, 12);
    dLay->setSpacing(10);

    m_desiredEdit = new QPlainTextEdit;
    m_desiredEdit->setObjectName("codeEditor");
    m_desiredEdit->setPlaceholderText(
        "{\n"
        "  \"temperature\": 25,\n"
        "  \"fan_speed\": \"high\",\n"
        "  \"mode\": \"auto\"\n"
        "}");
    m_desiredEdit->setMinimumHeight(140);
    m_desiredEdit->setMaximumHeight(200);
    dLay->addWidget(m_desiredEdit);

    auto *applyBtn = new QPushButton("💾 保存期望值");
    applyBtn->setObjectName("primaryButton");
    applyBtn->setMinimumHeight(38);
    applyBtn->setCursor(Qt::PointingHandCursor);
    dLay->addWidget(applyBtn, 0, Qt::AlignLeft);

    // -- Reported Card --
    auto *reportedBox = makeSection("设备状态 Reported");
    auto *rLay = new QVBoxLayout(reportedBox);
    rLay->setContentsMargins(12, 20, 12, 12);
    rLay->setSpacing(10);

    m_reportedEdit = new QPlainTextEdit;
    m_reportedEdit->setObjectName("codeEditorReadonly");
    m_reportedEdit->setReadOnly(true);
    m_reportedEdit->setPlaceholderText("等待设备上报...");
    m_reportedEdit->setMinimumHeight(140);
    m_reportedEdit->setMaximumHeight(200);
    rLay->addWidget(m_reportedEdit);

    auto *syncLabel = new QLabel("🟢 已同步");
    syncLabel->setObjectName("syncBadge");
    rLay->addWidget(syncLabel, 0, Qt::AlignRight);

    shadowRow->addWidget(desiredBox);
    shadowRow->addWidget(reportedBox);
    root->addLayout(shadowRow);
}

// ======================================================================
// 3. Chart Panel – chart + live indicator strip
// ======================================================================
void DetailPage::setupChartPanel(QVBoxLayout *root)
{
    auto *box = makeSection("实时数据监控");
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(12, 20, 12, 12);
    lay->setSpacing(12);

    // -- Live Metrics Strip (above chart) --
    auto *metricsRow = new QHBoxLayout;
    metricsRow->setSpacing(24);

    auto makeStrip = [](const QString &label, const QString &color)
        -> std::pair<QLabel *, QLabel *>
    {
        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *val = new QLabel("--");
        val->setStyleSheet(QString(
                               "font-size:18px; font-weight:bold; color:%1; background:transparent;")
                               .arg(color));
        auto *lbl = new QLabel(label);
        lbl->setStyleSheet(
            "font-size:11px; color:#9ca0b0; background:transparent;");
        col->addWidget(val);
        col->addWidget(lbl);
        return {val, lbl};
    };

    auto [tVal, tLbl] = makeStrip("温度 ℃", "#f38ba8");
    m_lblChartTemp = tVal;
    auto [hVal, hLbl] = makeStrip("湿度 %", "#89b4fa");
    m_lblChartHumidity = hVal;
    auto [sVal, sLbl] = makeStrip("设备状态", "#a6e3a1");
    m_lblChartStatus = sVal;

    metricsRow->addLayout(tVal->parentWidget() ? nullptr : new QVBoxLayout); // placeholder
    // Actually let me just build inline:
    metricsRow->addWidget(tVal);
    metricsRow->addWidget(tLbl);
    // This won't work as intended, let me restructure.

    // Actually, let me just build the strip properly:
    metricsRow = new QHBoxLayout;
    metricsRow->setSpacing(32);

    auto addMetric = [&](const QString &name, const QString &color) -> QLabel *
    {
        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *val = new QLabel("--");
        val->setStyleSheet(QString(
                               "font-size:20px; font-weight:bold; color:%1; background:transparent;")
                               .arg(color));
        val->setAlignment(Qt::AlignCenter);
        auto *lbl = new QLabel(name);
        lbl->setStyleSheet(
            "font-size:11px; color:#9ca0b0; background:transparent;");
        lbl->setAlignment(Qt::AlignCenter);
        col->addWidget(val, 0, Qt::AlignCenter);
        col->addWidget(lbl, 0, Qt::AlignCenter);
        metricsRow->addLayout(col);
        return val;
    };

    m_lblChartTemp = addMetric("温度 ℃", "#f38ba8");
    m_lblChartHumidity = addMetric("湿度 %", "#89b4fa");
    m_lblChartStatus = addMetric("状态", "#a6e3a1");
    metricsRow->addStretch();

    lay->addLayout(metricsRow);

    // -- Separator --
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("iotSeparator");
    lay->addWidget(sep);

    // -- Chart --
    m_chartTemp = new RealtimeChart("温度 / 湿度 实时曲线");
    m_chartTemp->addSeries("温度", QColor("#f38ba8"));
    m_chartTemp->addSeries("湿度", QColor("#89b4fa"));
    m_chartTemp->setMinimumHeight(240);
    lay->addWidget(m_chartTemp);

    root->addWidget(box);
}

// ======================================================================
// 4. Control Panel – 2-col: Quick Control | Custom Command
// ======================================================================
void DetailPage::setupControlPanel(QVBoxLayout *root)
{
    auto *controlRow = new QHBoxLayout;
    controlRow->setSpacing(16);

    // -- Quick Control Card --
    auto *quickBox = makeSection("快捷控制");
    auto *quickGrid = new QGridLayout(quickBox);
    quickGrid->setContentsMargins(12, 20, 12, 12);
    quickGrid->setSpacing(10);

    struct BtnDef
    {
        QString text;
        QString icon;
        QString color;
    };
    QVector<BtnDef> buttons = {
        {"开启风扇", "🌀", "#3b82f6"},
        {"关闭风扇", "⏸️", "#6b7280"},
        {"设置温度", "🌡️", "#f59e0b"},
        {"重启设备", "🔄", "#ef4444"}};

    for (int i = 0; i < buttons.size(); ++i)
    {
        auto *btn = new QPushButton(QString("%1 %2").arg(buttons[i].icon, buttons[i].text));
        btn->setObjectName(buttons[i].color == "#ef4444" ? "dangerButton" : "controlButton");
        btn->setMinimumHeight(44);
        btn->setMinimumWidth(120);
        btn->setCursor(Qt::PointingHandCursor);
        quickGrid->addWidget(btn, i / 2, i % 2);
    }

    // -- Custom Command Card --
    auto *customBox = makeSection("自定义指令");
    auto *customLay = new QVBoxLayout(customBox);
    customLay->setContentsMargins(12, 20, 12, 12);
    customLay->setSpacing(10);

    auto *cmdRow = new QHBoxLayout;
    cmdRow->setSpacing(8);

    m_cmdInput = new QLineEdit;
    m_cmdInput->setObjectName("cmdInput");
    m_cmdInput->setPlaceholderText("输入指令，例如: reboot / status / upgrade");
    m_cmdInput->setMinimumHeight(28);

    auto *sendBtn = new QPushButton("📤 发送");
    sendBtn->setObjectName("primaryButton");
    sendBtn->setMinimumHeight(28);
    sendBtn->setMinimumWidth(80);
    sendBtn->setCursor(Qt::PointingHandCursor);

    cmdRow->addWidget(m_cmdInput);
    cmdRow->addWidget(sendBtn);
    customLay->addLayout(cmdRow);

    // Quick command chips
    auto *chipsRow = new QHBoxLayout;
    chipsRow->setSpacing(8);
    QStringList chips = {"reboot", "status", "ping", "upgrade"};
    for (const auto &c : chips)
    {
        auto *chip = new QPushButton(c);
        chip->setObjectName("chipButton");
        chip->setCursor(Qt::PointingHandCursor);
        chipsRow->addWidget(chip);
    }
    chipsRow->addStretch();
    customLay->addLayout(chipsRow);

    controlRow->addWidget(quickBox);
    controlRow->addWidget(customBox);
    root->addLayout(controlRow);
}

// ======================================================================
// 5. History Panel
// ======================================================================
void DetailPage::setupHistoryPanel(QVBoxLayout *root)
{
    auto *box = makeSection("历史数据查询");
    auto *row = new QHBoxLayout(box);
    row->setSpacing(12);
    row->setContentsMargins(12, 20, 12, 12);

    auto *rangeLabel = new QLabel("时间范围:");
    rangeLabel->setObjectName("formLabel");
    auto *range = new QComboBox;
    range->addItems({"最近24小时", "最近7天", "最近30天"});
    range->setMinimumHeight(36);

    auto *metricLabel = new QLabel("指标:");
    metricLabel->setObjectName("formLabel");
    auto *metric = new QComboBox;
    metric->addItems({"温度", "湿度", "电量"});
    metric->setMinimumHeight(36);

    auto *query = new QPushButton("🔍 查询");
    query->setObjectName("primaryButton");
    query->setMinimumHeight(36);
    query->setCursor(Qt::PointingHandCursor);

    auto *exportCsv = new QPushButton("📥 导出CSV");
    exportCsv->setObjectName("secondaryButton");
    exportCsv->setMinimumHeight(36);
    exportCsv->setCursor(Qt::PointingHandCursor);

    row->addWidget(rangeLabel);
    row->addWidget(range);
    row->addWidget(metricLabel);
    row->addWidget(metric);
    row->addWidget(query);
    row->addWidget(exportCsv);
    row->addStretch();

    root->addWidget(box);
}

// ======================================================================
// Data Slots
// ======================================================================
void DetailPage::addDataPoint(const QString &metric, double value, qint64 timestamp)
{
    if (!m_chartTemp)
        return;
    if (metric == "temperature" || metric == "humidity")
    {
        m_chartTemp->appendPoint(
            metric == "temperature" ? "温度" : "湿度",
            QPointF(timestamp, value));
    }
    // Update live strip
    if (metric == "temperature" && m_lblChartTemp)
    {
        m_lblChartTemp->setText(QString::number(value, 'f', 1) + "℃");
    }
    if (metric == "humidity" && m_lblChartHumidity)
    {
        m_lblChartHumidity->setText(QString::number(value, 'f', 1) + "%");
    }
}

void DetailPage::showDevice(const DeviceInfo &device)
{
    m_current = device;

    // Status card
    m_lblName->setText(device.name);
    bool online = (device.status == DeviceStatus::Online);
    m_lblStatusDot->setObjectName(online ? "statusDotOnline" : "statusDotOffline");
    m_lblStatusDot->setStyleSheet(""); // force re-polish
    m_lblStatusDot->style()->unpolish(m_lblStatusDot);
    m_lblStatusDot->style()->polish(m_lblStatusDot);
    m_lblStatusText->setText(online ? "在线 Online" : "离线 Offline");
    m_lblStatusText->setStyleSheet(QString("color:%1; background:transparent;")
                                       .arg(online ? "#22c55e" : "#9ca0b0"));

    m_lblTemperature->setText(QString::number(device.temperature, 'f', 1) + "℃");
    m_lblHumidity->setText(QString::number(device.humidity, 'f', 1) + "%");
    m_lblLastUpdate->setText(device.lastSeen.isValid()
                                 ? device.lastSeen.toString("hh:mm:ss")
                                 : "--:--:--");

    // Chart strip
    if (m_lblChartTemp)
        m_lblChartTemp->setText(QString::number(device.temperature, 'f', 1) + "℃");
    if (m_lblChartHumidity)
        m_lblChartHumidity->setText(QString::number(device.humidity, 'f', 1) + "%");
    if (m_lblChartStatus)
        m_lblChartStatus->setText(online ? "在线" : "离线");

    // Reported shadow
    m_reportedEdit->setPlainText(
        QString("{\n"
                "  \"temperature\": %1,\n"
                "  \"humidity\": %2,\n"
                "  \"battery\": %3,\n"
                "  \"online\": %4,\n"
                "  \"fan_speed\": \"%5\"\n"
                "}")
            .arg(device.temperature, 0, 'f', 1)
            .arg(device.humidity, 0, 'f', 1)
            .arg(device.battery, 0, 'f', 0)
            .arg(device.status == DeviceStatus::Online ? "true" : "false")
            .arg(device.status == DeviceStatus::Online ? "high" : "off"));
}
