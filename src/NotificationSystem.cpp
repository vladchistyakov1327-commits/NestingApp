#include "NotificationSystem.h"

NotificationSystem::NotificationSystem(QObject *parent)
    : QObject(parent),
      m_salaryThreshold(500.0),
      m_payrollDay(20),
      m_showOnStartup(true) {}

void NotificationSystem::checkAll() {
    checkSalaryMismatch();
    checkPayrollReminder();
    checkLowMaterials();
    checkNegativeMargin();
    checkBirthdays();
}

void NotificationSystem::setSalaryThreshold(double rubles) {
    m_salaryThreshold = rubles;
}

void NotificationSystem::setPayrollDay(int day) {
    m_payrollDay = day;
}

void NotificationSystem::setShowOnStartup(bool show) {
    m_showOnStartup = show;
}

void NotificationSystem::checkSalaryMismatch() {
    // TODO: query database for salary differences
    // emit newNotification(SalaryMismatch, message, 1);
}

void NotificationSystem::checkPayrollReminder() {
    // TODO: calculate date differences and emit payroll reminders
}

void NotificationSystem::checkLowMaterials() {
    // TODO: inspect materials and min thresholds
}

void NotificationSystem::checkNegativeMargin() {
    // TODO: compute margins for products
}

void NotificationSystem::checkBirthdays() {
    // TODO: check employee birthdays
}
