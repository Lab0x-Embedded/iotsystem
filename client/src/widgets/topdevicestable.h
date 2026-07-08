#ifndef TOPDEVICESTABLE_H
#define TOPDEVICESTABLE_H

#include <QWidget>
#include "models/devicemodel.h"

class QGridLayout;
class QLabel;

class TopDevicesTable : public QWidget {
    Q_OBJECT
public:
    explicit TopDevicesTable(QWidget *parent = nullptr);

public slots:
    void updateTopDevices(const QVector<DeviceInfo> &devices, int column);

private:
    QGridLayout *m_grid;
    int m_column;
};
#endif
