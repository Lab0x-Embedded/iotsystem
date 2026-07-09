#include "gaugewidget.h"
#include "theme/theme.h"
#include <QPainter>
#include <QtMath>
#include <QFontMetrics>

/* ============================================
 * GaugeWidget - Circular Gauge Display
 * Following Qt UI Design Guidelines:
 * - Typography: Clear hierarchy (value > label)
 * - Color + Shape: Visual state communication
 * - Aesthetic-Usability Effect: Polished design
 * ============================================ */

GaugeWidget::GaugeWidget(const QString &label, QWidget *parent)
    : QWidget(parent), m_label(label) {
    setMinimumSize(160, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Connect to theme changes
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme(ThemeManager::instance()->isDark());
    });
}

void GaugeWidget::setValue(double v) { 
    m_value = v; 
    update(); 
}

void GaugeWidget::setRange(double mn, double mx) { 
    m_min = mn; 
    m_max = mx; 
    update(); 
}

void GaugeWidget::setUnit(const QString &u) { 
    m_unit = u; 
}

void GaugeWidget::setColor(const QColor &c) { 
    m_color = c; 
    update(); 
}

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
    
    int side = qMin(width(), height()) - 24;  // More padding for better spacing
    QRectF r((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    
    double ratio = (m_max > m_min) ? qBound(0.0, (m_value - m_min) / (m_max - m_min), 1.0) : 0.0;
    
    // Draw background groove
    p.setPen(QPen(m_groove, 12, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 225 * 16, -270 * 16);
    
    // Draw value arc
    p.setPen(QPen(m_color, 12, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 225 * 16, -270 * 16 * ratio);
    
    // Draw value text - Primary typography level
    p.setPen(m_labelColor.darker(115));
    QFont valueFont("PingFang SC", 20, QFont::Bold);
    p.setFont(valueFont);
    QString valTxt = QString::number(m_value, 'f', 1) + m_unit;
    
    // Center the value text
    QFontMetrics fm(valueFont);
    QRectF textRect = fm.boundingRect(valTxt);
    QPointF center = r.center();
    p.drawText(QPointF(center.x() - textRect.width() / 2, center.y() - 4), valTxt);
    
    // Draw label text - Secondary typography level
    p.setPen(m_labelColor);
    QFont labelFont("PingFang SC", 11, QFont::Normal);
    p.setFont(labelFont);
    
    QFontMetrics labelFm(labelFont);
    QRectF labelRect = labelFm.boundingRect(m_label);
    p.drawText(QPointF(center.x() - labelRect.width() / 2, center.y() + 20), m_label);
}
