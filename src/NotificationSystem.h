#ifndef NOTIFICATIONSYSTEM_H
#define NOTIFICATIONSYSTEM_H

#include <QObject>
#include <QString>
#include <QDate>

class NotificationSystem : public QObject {
    Q_OBJECT
public:
    enum NotificationType {
        SalaryMismatch,
        PayrollReminder,
        LowMaterial,
        NegativeMargin,
        Birthday
    };

    NotificationSystem(QObject *parent = nullptr);

    void checkAll();
    void setSalaryThreshold(double rubles);
    void setPayrollDay(int day);
    void setShowOnStartup(bool show);

signals:
    void newNotification(NotificationType type, const QString &message, int priority);

private slots:
    void checkSalaryMismatch();
    void checkPayrollReminder();
    void checkLowMaterials();
    void checkNegativeMargin();
    void checkBirthdays();

private:
    double m_salaryThreshold;
    int m_payrollDay;
    bool m_showOnStartup;
};

#endif // NOTIFICATIONSYSTEM_H
