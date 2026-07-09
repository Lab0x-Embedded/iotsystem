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

/* ============================================
 * DashboardCard - Stats Card Widget
 * Following Qt UI Design Guidelines:
 * - Aesthetic-Usability Effect: Polished design
 * - Uniform Connectedness: Visual grouping
 * - von Restorff Effect: Accent color for emphasis
 * ============================================ */
class DashboardCard : public QFrame {
public:
    DashboardCard(const QString &title, const QString &value, const QColor &accent, QWidget *parent = nullptr)
        : QFrame(parent), m_accent(accent) {
        setObjectName("card");
        setFrameShape(QFrame::StyledPanel);
        updateTheme();
        
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(16, 12, 16, 12);
        lay->setSpacing(8);
        
        QLabel *t = new QLabel(title);
        t->setProperty("title", true);
        t->setObjectName("cardTitle");
        
        QLabel *v = new QLabel(value);
        v->setProperty("value", true);
        v->setObjectName("cardValue");
        v->setStyleSheet(QString("color:%1; font-size:28px; font-weight:bold;").arg(accent.name()));
        
        lay->addWidget(t);
        lay->addWidget(v);
        
        // Connect to theme changes
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &DashboardCard::updateTheme);
    }
    
    void setValue(const QString &value) {
        // Find the value label and update it
        QList<QLabel*> labels = findChildren<QLabel*>();
        for (QLabel *label : labels) {
            if (label->property("value").toBool()) {
                label->setText(value);
                break;
            }
        }
    }

private:
    void updateTheme() {
        const Theme &theme = ThemeManager::instance()->current();
        setStyleSheet(QString("QFrame#card { background-color: %1; border: 1px solid %2; border-radius: 12px; }")
                          .arg(theme.cardBg, theme.cardBorder));
    }
    
    QColor m_accent;
};

OverviewPage::OverviewPage(DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // Stats cards row
    auto *statsRow = new QHBoxLayout;
    statsRow->setSpacing(12);
    
    m_cardTotal = new DashboardCard("设备总数", "0", QColor("#89b4fa"));
    m_cardOnline = new DashboardCard("在线设备", "0", QColor("#a6e3a1"));
    m_cardOffline = new DashboardCard("离线设备", "0", QColor("#9399b2"));
    m_cardAlarm = new DashboardCard("告警设备", "0", QColor("#f38ba8"));
    
    statsRow->addWidget(m_cardTotal);
    statsRow->addWidget(m_cardOnline);
    statsRow->addWidget(m_cardOffline);
    statsRow->addWidget(m_cardAlarm);
    root->addLayout(statsRow);

    // Filter row
    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    
    m_search = new QLineEdit;
    m_search->setPlaceholderText("搜索设备 ID/名称...");
    m_search->setMinimumHeight(36);
    
    m_filterStatus = new QComboBox;
    m_filterStatus->addItems({"全部状态", "在线", "离线", "告警"});
    m_filterStatus->setMinimumHeight(36);
    
    filterRow->addWidget(new QLabel("筛选:"));
    filterRow->addWidget(m_filterStatus);
    filterRow->addStretch();
    filterRow->addWidget(new QLabel("搜索:"));
    filterRow->addWidget(m_search);
    root->addLayout(filterRow);

    // Device table
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMinimumHeight(300);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (!idx.isValid()) return;
        emit deviceSelected(m_model->deviceAt(idx.row()).id);
    });
}

void OverviewPage::updateCounts(int total, int online, int alarm) {
    m_cardTotal->setValue(QString::number(total));
    m_cardOnline->setValue(QString::number(online));
    m_cardAlarm->setValue(QString::number(alarm));
    m_cardOffline->setValue(QString::number(total - online - alarm));
}
