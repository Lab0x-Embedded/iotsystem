#ifndef GAUGEWIDGET_H
#define GAUGEWIDGET_H

#include <QWidget>
#include <QString>

class GaugeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GaugeWidget(const QString &label, QWidget *parent = nullptr);

    void setValue(double v);
    void setRange(double min, double max);
    void setUnit(const QString &unit);
    void setColor(const QColor &c);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_label;
    double m_value = 0, m_min = 0, m_max = 100;
    QString m_unit;
    QColor m_color = QColor("#89b4fa");
};
#endif
