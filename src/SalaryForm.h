#ifndef SALARYFORM_H
#define SALARYFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>
#include <QComboBox>
#include <QDateEdit>

class SalaryForm : public QWidget {
    Q_OBJECT
public:
    SalaryForm(QWidget *parent = nullptr);

private slots:
    void addSalary();
    void deleteSalary();
    void saveSalary();
    void markAsPaid();
    void calculateDifference();

private:
    void setupUI();
    void loadSalary();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_paidBtn;
    QSqlTableModel *m_model;
};

#endif // SALARYFORM_H
