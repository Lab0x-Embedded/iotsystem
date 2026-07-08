#ifndef DETAILPAGE_H
#define DETAILPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include "models/devicemodel.h"

class RealtimeChart;
class QLabel;
class QGroupBox;
class QPushButton;
class QTextEdit;
class QLineEdit;

class DetailPage : public QWidget {
    Q_OBJECT
public:
    explicit DetailPage(QWidget *parent = nullptr);

public slots:
    void showDevice(const DeviceInfo &device);
    void addDataPoint(const QString &metric, double value, qint64 timestamp);

private:
    DeviceInfo m_current;
    QLabel *m_lblName;
    QLabel *m_lblStatus;
    QTextEdit *m_desiredEdit;
    QTextEdit *m_reportedEdit;
    RealtimeChart *m_chartTemp;
    QLineEdit *m_cmdInput;

    void setupShadowPanel(QVBoxLayout *root);
    void setupChartPanel(QVBoxLayout *root);
    void setupControlPanel(QVBoxLayout *root);
    void setupHistoryPanel(QVBoxLayout *root);
};
#endif
