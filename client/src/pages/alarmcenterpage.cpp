#include "alarmcenterpage.h"
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>

AlarmCenterPage::AlarmCenterPage(AlarmModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(new QLabel("<b>告警中心</b>"));
    titleRow->addStretch();
    titleRow->addWidget(new QLabel("查看已处理"));
    root->addLayout(titleRow);

    auto *filterRow = new QHBoxLayout;
    m_filterSeverity = new QComboBox;
    m_filterSeverity->addItems({"全部", "CRITICAL", "WARNING", "INFO"});
    m_filterSeverity->setStyleSheet("background:#11111b; color:#cdd6f4; padding:6px;");
    m_ackButton = new QPushButton("确认选中");
    m_ackButton->setStyleSheet("background:#89b4fa; color:#1e1e2e; font-weight:bold; border:none; border-radius:6px; padding:6px;");
    filterRow->addWidget(new QLabel("筛选:"));
    filterRow->addWidget(m_filterSeverity);
    filterRow->addStretch();
    filterRow->addWidget(m_ackButton);
    filterRow->setSpacing(8);
    root->addLayout(filterRow);

    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setStyleSheet("QTableView{background:#1e1e2e; color:#cdd6f4; gridline-color:#313244;}"
                          "QHeaderView::section{background:#11111b; color:#a6adc8; border:none; padding:8px; font-weight:bold;}");
    root->addWidget(m_table, 1);

    connect(m_filterSeverity, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlarmCenterPage::filterChanged);
    connect(m_ackButton, &QPushButton::clicked, this, &AlarmCenterPage::ackSelected);
}

void AlarmCenterPage::filterChanged() {
    QString severity = m_filterSeverity->currentText();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        m_table->setRowHidden(i, severity != "全部" && m_model->at(i).severityText() != severity);
    }
}

void AlarmCenterPage::ackSelected() {
    QModelIndexList sel = m_table->selectionModel()->selectedRows();
    for (const auto &idx : sel) m_model->acknowledge(idx.row());
}
