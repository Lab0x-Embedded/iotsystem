#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

class NavSidebar;
class DataManager;
class OverviewPage;
class DetailPage;
class DashboardPage;
class AlarmCenterPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void switchPage(int index);
    void showDeviceDetail(const QString &deviceId);

private:
    NavSidebar *m_sidebar;
    QStackedWidget *m_stack;
    DataManager *m_data;
    OverviewPage *m_overview;
    DetailPage *m_detail;
    DashboardPage *m_dashboard;
    AlarmCenterPage *m_alarm;
};
#endif
