#ifndef EMPLOYEESFORM_H
#define EMPLOYEESFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>

class EmployeesForm : public QWidget {
    Q_OBJECT
public:
    EmployeesForm(QWidget *parent = nullptr);

private slots:
    void addEmployee();
    void deleteEmployee();
    void saveEmployees();

private:
    void setupUI();
    void loadEmployees();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_saveBtn;
    QSqlTableModel *m_model;
};

#endif // EMPLOYEESFORM_H
