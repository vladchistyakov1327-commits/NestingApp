#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>

class MaterialsForm;
class ReceiptForm;
class ShipmentForm;
class MovementForm;
class EmployeesForm;
class SalaryForm;
class PerformedForm;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private:
    void setupUI();

    QTabWidget *m_tabWidget;
    MaterialsForm *m_materialsForm;
    ReceiptForm *m_receiptForm;
    ShipmentForm *m_shipmentForm;
    MovementForm *m_movementForm;
    EmployeesForm *m_employeesForm;
    SalaryForm *m_salaryForm;
    PerformedForm *m_performedForm;
};

#endif // MAINWINDOW_H
