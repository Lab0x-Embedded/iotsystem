#include "navsidebar.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

NavSidebar::NavSidebar(QWidget *parent) : QWidget(parent) {
    setObjectName("NavSidebar");
    setFixedWidth(200);
    setStyleSheet("NavSidebar{background:#181825; border-right:1px solid #313244;}");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 16, 0, 0);
    root->setSpacing(2);

    QLabel *title = new QLabel("IoT Manager");
    title->setStyleSheet("color:#89b4fa; font-size:16px; font-weight:bold; padding:0 16px 16px;");
    root->addWidget(title);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    addEntry("📊 设备总览", 0);
    addEntry("🔍 设备详情", 1);
    addEntry("📈 数据大屏", 2);
    addEntry("🚨 告警中心", 3);
    root->addStretch();

    connect(m_group, QOverload<int>::of(&QButtonGroup::idClicked), this, &NavSidebar::pageSelected);
    m_group->button(0)->setChecked(true);
}

void NavSidebar::addEntry(const QString &text, int index) {
    QPushButton *btn = new QPushButton(text);
    btn->setCheckable(true);
    btn->setMinimumHeight(44);
    btn->setStyleSheet(
        "QPushButton{background:transparent; color:#a6adc8; text-align:left; padding:0 16px; border:none; border-left:3px solid transparent; font-size:14px;}"
        "QPushButton:hover{background:#313244; color:#cdd6f4;}"
        "QPushButton:checked{background:#313244; color:#89b4fa; border-left:3px solid #89b4fa; font-weight:bold;}"
    );
    m_group->addButton(btn, index);
    layout()->addWidget(btn);
}
