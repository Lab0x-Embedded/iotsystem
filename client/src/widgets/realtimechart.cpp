#include "realtimechart.h"
#include <QtCharts>
#include <QDateTime>

RealtimeChart::RealtimeChart(const QString &title, QWidget *parent)
    : QChartView(parent) {
    QChart *chart = new QChart();
    chart->setTitle(title);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setTheme(QChart::ChartThemeDark);

    m_axisX = new QDateTimeAxis;
    m_axisX->setFormat("hh:mm:ss");
    m_axisX->setTitleText("时间");
    chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis;
    m_axisY->setTitleText("值");
    chart->addAxis(m_axisY, Qt::AlignLeft);

    setChart(chart);
    setRenderHint(QPainter::Antialiasing);
}

void RealtimeChart::addSeries(const QString &name, const QColor &color) {
    auto *s = new QLineSeries();
    s->setName(name);
    QPen pen(color); pen.setWidth(2); s->setPen(pen);
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
    if (dark) {
        c->setTheme(QChart::ChartThemeDark);
        c->setBackgroundBrush(QColor("#1e1e2e"));
        c->setTitleBrush(QColor("#cdd6f4"));
        c->legend()->setLabelColor(QColor("#a6adc8"));
        m_axisX->setLabelsColor(QColor("#a6adc8"));
        m_axisY->setLabelsColor(QColor("#a6adc8"));
        m_axisX->setTitleBrush(QColor("#cdd6f4"));
        m_axisY->setTitleBrush(QColor("#cdd6f4"));
        m_axisX->setLinePenColor(QColor("#313244"));
        m_axisY->setLinePenColor(QColor("#313244"));
    } else {
        c->setTheme(QChart::ChartThemeLight);
        c->setBackgroundBrush(QColor("#eff1f5"));
        c->setTitleBrush(QColor("#4c4f69"));
        c->legend()->setLabelColor(QColor("#5c5f77"));
        m_axisX->setLabelsColor(QColor("#5c5f77"));
        m_axisY->setLabelsColor(QColor("#5c5f77"));
        m_axisX->setTitleBrush(QColor("#4c4f69"));
        m_axisY->setTitleBrush(QColor("#4c4f69"));
        m_axisX->setLinePenColor(QColor("#bcc0cc"));
        m_axisY->setLinePenColor(QColor("#bcc0cc"));
    }
}
