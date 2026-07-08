#ifndef REALTIMECHART_H
#define REALTIMECHART_H

#include <QChartView>
#include <QLineSeries>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QHash>

class RealtimeChart : public QChartView {
    Q_OBJECT
public:
    explicit RealtimeChart(const QString &title, QWidget *parent = nullptr);

    void addSeries(const QString &name, const QColor &color);
    void appendPoint(const QString &name, const QPointF &p);
    void trim(int maxPoints);

private:
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisY;
    QHash<QString, QLineSeries*> m_series;
    int m_maxPoints = 360;
};
#endif
