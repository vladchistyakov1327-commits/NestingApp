#ifndef EMPLOYEERATING_H
#define EMPLOYEERATING_H

#include <QString>
#include <QVector>

class EmployeeRating {
public:
    struct RatingEntry {
        QString name;
        double earnings;
        int operationCount;
        double bonus;
    };

    EmployeeRating();

    QVector<RatingEntry> calculateCurrentMonth();
    QVector<RatingEntry> historyForMonth(int month, int year);

    // helper methods for badges, plans etc.
    void updatePersonalPlan(const QString &employee, double plan);
};

#endif // EMPLOYEERATING_H
