#include "realtimechart.h"
#include "theme/theme.h"
#include <QtCharts>
#include <QDateTime>

/* ============================================
 * RealtimeChart - Live Data Visualization
 * Following Qt UI Design Guidelines:
 * - Doherty Threshold: Visual feedback within 400ms
 * - Progressive Disclosure: Show relevant data
 * - Color + Shape: Visual state communication
 * ============================================ */

RealtimeChart::RealtimeChart(const QString &title, QWidget *parent)
    : QChartView(parent) {
    QChart *chart = new QChart();
    chart->setTitle(title);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setDropShadowEnabled(true);

    // Apply initial theme
    bool dark = ThemeManager::instance()->isDark();

    m_axisX = new QDateTimeAxis;
    m_axisX->setFormat("hh:mm:ss");
    m_axisX->setTitleText("时间");
    m_axisX->setLabelsFont(QFont("PingFang SC", 10));
    chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis;
    m_axisY->setTitleText("值");
    m_axisY->setLabelsFont(QFont("PingFang SC", 10));
    chart->addAxis(m_axisY, Qt::AlignLeft);

    // Apply initial theme AFTER axes are created
    setDarkTheme(dark);

    setChart(chart);
    setRenderHint(QPainter::Antialiasing);

    // Connect to theme changes
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        setDarkTheme(ThemeManager::instance()->isDark());
    });
}

void RealtimeChart::addSeries(const QString &name, const QColor &color) {
    auto *s = new QLineSeries();
    s->setName(name);
    QPen pen(color);
    pen.setWidth(2);
    s->setPen(pen);
    chart()->addSeries(s);
    s->attachAxis(m_axisX);
    s->attachAxis(m_axisY);
    m_series[name] = s;
}

void RealtimeChart::appendPoint(const QString &name, const QPointF &p) {
    if (!m_series.contains(name)) return;
    m_series[name]->append(p);
    trim(m_maxPoints);
    if (!m_series[name]->points().isEmpty()) {
        const auto &pts = m_series[name]->points();
        m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pts.first().x())),
                          QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pts.last().x())));
    }
}

void RealtimeChart::trim(int maxPoints) {
    m_maxPoints = maxPoints;
    for (auto *s : m_series) {
        while (s->count() > m_maxPoints) s->remove(0);
    }
}

void RealtimeChart::setDarkTheme(bool dark) {
    QChart *c = chart();
    if (!c || !m_axisX || !m_axisY) return;  // Safety check

    if (dark) {
        // Dark theme - Catppuccin Mocha
        c->setTheme(QChart::ChartThemeDark);
        c->setBackgroundBrush(QColor("#1e1e2e"));
        c->setTitleBrush(QColor("#cdd6f4"));
        c->setTitleFont(QFont("PingFang SC", 14, QFont::Bold));
        c->legend()->setLabelColor(QColor("#a6adc8"));
        c->legend()->setFont(QFont("PingFang SC", 10));

        m_axisX->setLabelsColor(QColor("#a6adc8"));
        m_axisY->setLabelsColor(QColor("#a6adc8"));
        m_axisX->setTitleBrush(QColor("#cdd6f4"));
        m_axisY->setTitleBrush(QColor("#cdd6f4"));
        m_axisX->setLinePenColor(QColor("#313244"));
        m_axisY->setLinePenColor(QColor("#313244"));
        m_axisX->setGridLineColor(QColor("#313244"));
        m_axisY->setGridLineColor(QColor("#313244"));
    } else {
        // Light theme - Catppuccin Latte
        c->setTheme(QChart::ChartThemeLight);
        c->setBackgroundBrush(QColor("#f8fafc"));
        c->setTitleBrush(QColor("#334155"));
        c->setTitleFont(QFont("PingFang SC", 14, QFont::Bold));
        c->legend()->setLabelColor(QColor("#64748b"));
        c->legend()->setFont(QFont("PingFang SC", 10));

        m_axisX->setLabelsColor(QColor("#64748b"));
        m_axisY->setLabelsColor(QColor("#64748b"));
        m_axisX->setTitleBrush(QColor("#334155"));
        m_axisY->setTitleBrush(QColor("#334155"));
        m_axisX->setLinePenColor(QColor("#e2e8f0"));
        m_axisY->setLinePenColor(QColor("#e2e8f0"));
        m_axisX->setGridLineColor(QColor("#e2e8f0"));
        m_axisY->setGridLineColor(QColor("#e2e8f0"));
    }
}
