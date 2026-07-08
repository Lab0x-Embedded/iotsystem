#include "navsidebar.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "theme/theme.h"

NavSidebar::NavSidebar(QWidget *parent) : QWidget(parent) {
    setObjectName("NavSidebar");
    setFixedWidth(210);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 18, 0, 0);
    root->setSpacing(2);

    QLabel *title = new QLabel("IoT Device Manager");
    title->setObjectName("sidebarTitle");
    root->addWidget(title);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    addEntry("📊  设备总览", 0);
    addEntry("🔍  设备详情", 1);
    addEntry("📈  数据大屏", 2);
    addEntry("🚨  告警中心", 3);
    root->addStretch();

    m_themeToggle = new QPushButton("🌙  深色模式");
    m_themeToggle->setObjectName("themeToggle");
    m_themeToggle->setToolTip("切换深色/浅色模式");
    root->addWidget(m_themeToggle, 0, Qt::AlignHCenter);
    root->setContentsMargins(12, 18, 12, 12);

    connect(m_themeToggle, &QPushButton::clicked, ThemeManager::instance(), &ThemeManager::toggle);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &NavSidebar::updateToggleLabel);
    updateToggleLabel();

    connect(m_group, QOverload<int>::of(&QButtonGroup::idClicked), this, &NavSidebar::pageSelected);
    m_group->button(0)->setChecked(true);
}

void NavSidebar::addEntry(const QString &text, int index) {
    QPushButton *btn = new QPushButton(text);
    btn->setCheckable(true);
    btn->setMinimumHeight(44);
    m_group->addButton(btn, index);
    layout()->addWidget(btn);
}

void NavSidebar::updateToggleLabel() {
    if (ThemeManager::instance()->isDark()) {
        m_themeToggle->setText("☀️  浅色模式");
    } else {
        m_themeToggle->setText("🌙  深色模式");
    }
}
