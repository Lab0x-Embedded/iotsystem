#include "statusindicator.h"
#include <QPainter>

StatusIndicator::StatusIndicator(QWidget *parent) : QWidget(parent) {
    setFixedSize(12, 12);
}
void StatusIndicator::setStatus(DeviceStatus status) {
    if (m_status != status) { m_status = status; update(); }
}
void StatusIndicator::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    DeviceInfo d; d.status = m_status;
    p.setBrush(d.statusColor());
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect());
}
