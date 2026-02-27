#include "EmployeeRating.h"

EmployeeRating::EmployeeRating() {
}

QVector<EmployeeRating::RatingEntry> EmployeeRating::calculateCurrentMonth() {
    QVector<RatingEntry> list;
    // TODO: query "Выполнено" table and aggregate by employee
    return list;
}

QVector<EmployeeRating::RatingEntry> EmployeeRating::historyForMonth(int month, int year) {
    QVector<RatingEntry> list;
    // TODO: load historical data from archive
    return list;
}

void EmployeeRating::updatePersonalPlan(const QString &employee, double plan) {
    // TODO: store plan in штат data
}
