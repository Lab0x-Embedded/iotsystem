#include "alarmcenterpage.h"
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>

/* ============================================
 * AlarmCenterPage - Alarm Management
 * Following Qt UI Design Guidelines:
 * - Color + Shape + Text: Alarm severity indicators
 * - Recognition Over Recall: Visible actions
 * - Doherty Threshold: Quick feedback
 * ============================================ */

AlarmCenterPage::AlarmCenterPage(AlarmModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // Title row
    auto *titleRow = new QHBoxLayout;
    auto *titleLabel = new QLabel("告警中心");
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold;");
    
    auto *viewAcked = new QLabel("查看已处理");
    viewAcked->setStyleSheet("color: #89b4fa; cursor: pointer;");
    
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(viewAcked);
    root->addLayout(titleRow);

    // Filter row
    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    
    m_filterSeverity = new QComboBox;
    m_filterSeverity->addItems({"全部级别", "CRITICAL", "WARNING", "INFO"});
    m_filterSeverity->setMinimumHeight(36);
    
    m_ackButton = new QPushButton("确认选中");
    m_ackButton->setObjectName("primaryButton");
    m_ackButton->setMinimumHeight(36);
    
    auto *filterLabel = new QLabel("筛选:");
    filterLabel->setStyleSheet("font-weight:bold;");
    
    filterRow->addWidget(filterLabel);
    filterRow->addWidget(m_filterSeverity);
    filterRow->addStretch();
    filterRow->addWidget(m_ackButton);
    root->addLayout(filterRow);

    // Alarm table
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMinimumHeight(300);
    root->addWidget(m_table, 1);

    connect(m_filterSeverity, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlarmCenterPage::filterChanged);
    connect(m_ackButton, &QPushButton::clicked, this, &AlarmCenterPage::ackSelected);
}

void AlarmCenterPage::filterChanged() {
    QString severity = m_filterSeverity->currentText();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        m_table->setRowHidden(i, severity != "全部级别" && m_model->at(i).severityText() != severity);
    }
}

void AlarmCenterPage::ackSelected() {
    QModelIndexList sel = m_table->selectionModel()->selectedRows();
    for (const auto &idx : sel) {
        m_model->acknowledge(idx.row());
    }
}
