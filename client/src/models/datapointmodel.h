#ifndef DATAPOINTMODEL_H
#define DATAPOINTMODEL_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QPointF>

class DataPointModel : public QObject {
    Q_OBJECT
public:
    explicit DataPointModel(QObject *parent = nullptr);

    void appendSeries(const QString &name, const QVector<QPointF> &series);
    void appendPoint(const QString &name, const QPointF &p);
    void trimSeries(const QString &name, int maxPoints);

    QStringList seriesNames() const { return m_series.keys(); }
    const QVector<QPointF> series(const QString &name) const { return m_series.value(name); }

signals:
    void seriesAppended(const QString &name, const QPointF &point);

private:
    QMap<QString, QVector<QPointF>> m_series;
};
#endif
