#include "overviewpage.h"
#include <QTableView>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include "widgets/statusindicator.h"
#include "theme/theme.h"

class DashboardCard : public QFrame {
public:
    DashboardCard(const QString &title, const QString &value, const QColor &accent, QWidget *parent = nullptr)
        : QFrame(parent) {
        setObjectName("card");
        setFrameShape(QFrame::StyledPanel);
        const Theme &theme = ThemeManager::instance()->current();
        setStyleSheet(QString("DashboardCard{background-color:%1; border-left:4px solid %2;}")
                          .arg(theme.cardBg, accent.name()));
        auto *lay = new QVBoxLayout(this);
        QLabel *t = new QLabel(title);
        t->setProperty("title", true);
        t->setStyleSheet(QString("font-size:13px; font-weight:normal;"));
        QLabel *v = new QLabel(value);
        v->setProperty("value", true);
        v->setStyleSheet(QString("color:%1; font-size:28px; font-weight:bold;").arg(accent.name()));
        lay->addWidget(t); lay->addWidget(v);
    }
};

OverviewPage::OverviewPage(DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);

    auto *statsRow = new QHBoxLayout;
    m_statTotal = new QLabel("0"); m_statOnline = new QLabel("0");
    m_statOffline = new QLabel("0"); m_statAlarm = new QLabel("0");
    statsRow->addWidget(new DashboardCard("设备总数", "0", QColor("#89b4fa")));
    statsRow->addWidget(new DashboardCard("在线设备", "0", QColor("#a6e3a1")));
    statsRow->addWidget(new DashboardCard("离线设备", "0", QColor("#9399b2")));
    statsRow->addWidget(new DashboardCard("告警设备", "0", QColor("#f38ba8")));
    statsRow->setSpacing(12);
    root->addLayout(statsRow);

    auto *filterRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText("搜索设备 ID/名称...");
    m_filterStatus = new QComboBox;
    m_filterStatus->addItems({"全部", "在线", "离线", "告警"});
    filterRow->addWidget(new QLabel("筛选:"));
    filterRow->addWidget(m_filterStatus);
    filterRow->addStretch();
    filterRow->addWidget(new QLabel("搜索:"));
    filterRow->addWidget(m_search);
    filterRow->setSpacing(8);
    root->addLayout(filterRow);

    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (!idx.isValid()) return;
        emit deviceSelected(m_model->deviceAt(idx.row()).id);
    });
}

void OverviewPage::updateCounts(int total, int online, int alarm) {
    m_statTotal->setText(QString::number(total));
    m_statOnline->setText(QString::number(online));
    m_statAlarm->setText(QString::number(alarm));
    m_statOffline->setText(QString::number(total - online - alarm));
}
