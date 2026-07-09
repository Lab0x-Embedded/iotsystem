#include "statusindicator.h"
#include <QPainter>

/* ============================================
 * StatusIndicator - Device Status Display
 * Following Qt UI Design Guidelines:
 * - Color + Shape + Text: Visual state communication
 * - von Restorff Effect: Distinct visual indicator
 * ============================================ */

StatusIndicator::StatusIndicator(QWidget *parent) : QWidget(parent) {
    setFixedSize(12, 12);
    setToolTip("设备状态指示器");
}

void StatusIndicator::setStatus(DeviceStatus status) {
    if (m_status != status) {
        m_status = status;
        update();
    }
}

void StatusIndicator::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    DeviceInfo d;
    d.status = m_status;
    
    // Draw status indicator with appropriate color
    p.setBrush(d.statusColor());
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect());
}
