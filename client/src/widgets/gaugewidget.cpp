#include "gaugewidget.h"
#include <QPainter>
#include <QtMath>

GaugeWidget::GaugeWidget(const QString &label, QWidget *parent)
    : QWidget(parent), m_label(label) {
    setMinimumSize(160, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void GaugeWidget::setValue(double v) { m_value = v; update(); }
void GaugeWidget::setRange(double mn, double mx) { m_min = mn; m_max = mx; update(); }
void GaugeWidget::setUnit(const QString &u) { m_unit = u; }
void GaugeWidget::setColor(const QColor &c) { m_color = c; update(); }

void GaugeWidget::applyTheme(bool dark) {
    if (dark) {
        m_groove = QColor("#313244");
        m_labelColor = QColor("#a6adc8");
    } else {
        m_groove = QColor("#ccd0da");
        m_labelColor = QColor("#5c5f77");
    }
    update();
}

void GaugeWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int side = qMin(width(), height()) - 16;
    QRectF r((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    double ratio = (m_max > m_min) ? qBound(0.0, (m_value - m_min) / (m_max - m_min), 1.0) : 0.0;
    p.setPen(QPen(m_groove, 12, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 225 * 16, -270 * 16);
    p.setPen(QPen(m_color, 12, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 225 * 16, -270 * 16 * ratio);
    p.setPen(m_labelColor.darker(115));
    p.setFont(QFont("Arial", 18, QFont::Bold));
    QString valTxt = QString::number(m_value, 'f', 1) + m_unit;
    p.drawText(r, Qt::AlignCenter, valTxt);
    p.setPen(m_labelColor);
    p.setFont(QFont("Arial", 10));
    p.drawText(r.adjusted(0, 40, 0, 0), Qt::AlignHCenter | Qt::AlignTop, m_label);
}
