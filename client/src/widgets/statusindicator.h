#ifndef STATUSINDICATOR_H
#define STATUSINDICATOR_H

#include <QWidget>
#include "models/devicemodel.h"

class StatusIndicator : public QWidget {
    Q_OBJECT
public:
    explicit StatusIndicator(QWidget *parent = nullptr);
    void setStatus(DeviceStatus status);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    DeviceStatus m_status = DeviceStatus::Offline;
};
#endif
