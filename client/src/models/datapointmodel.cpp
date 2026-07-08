#include "datapointmodel.h"

DataPointModel::DataPointModel(QObject *parent) : QObject(parent) {}

void DataPointModel::appendSeries(const QString &name, const QVector<QPointF> &series) {
    m_series[name] = series;
}

void DataPointModel::appendPoint(const QString &name, const QPointF &p) {
    m_series[name].push_back(p);
    emit seriesAppended(name, p);
}

void DataPointModel::trimSeries(const QString &name, int maxPoints) {
    if (!m_series.contains(name)) return;
    auto &v = m_series[name];
    if (v.size() > maxPoints) {
        v.erase(v.begin(), v.end() - maxPoints);
    }
}
