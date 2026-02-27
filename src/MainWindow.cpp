#include "MainWindow.h"
#include "MaterialsForm.h"
#include "ReceiptForm.h"
#include "ShipmentForm.h"
#include "MovementForm.h"
#include "EmployeesForm.h"
#include "SalaryForm.h"
#include "PerformedForm.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    
    // initialize database
    if (!DatabaseManager::instance().open("production.db")) {
        QMessageBox::critical(this, "Ошибка БД", "Не удалось открыть базу данных");
    }
    
    setupUI();
    
    setWindowTitle("Production Manager Pro - Управление Производством");
    resize(1200, 700);
}

void MainWindow::setupUI() {
    m_tabWidget = new QTabWidget(this);
    
    // create forms
    m_materialsForm = new MaterialsForm();
    m_receiptForm = new ReceiptForm();
    m_shipmentForm = new ShipmentForm();
    m_movementForm = new MovementForm();
    m_employeesForm = new EmployeesForm();
    m_salaryForm = new SalaryForm();
    m_performedForm = new PerformedForm();
    
    // add tabs - inventory
    m_tabWidget->addTab(m_materialsForm, "Справочник материалов");
    m_tabWidget->addTab(m_receiptForm, "Приход материалов");
    m_tabWidget->addTab(m_shipmentForm, "Отгрузка");
    m_tabWidget->addTab(m_movementForm, "Движение материалов");
    
    // add tabs - employees and salary
    m_tabWidget->addTab(m_employeesForm, "Штат");
    m_tabWidget->addTab(m_salaryForm, "Зарплата");
    m_tabWidget->addTab(m_performedForm, "Выполнено");
    
    // TODO: add more tabs for ratings, notifications, products
    
    setCentralWidget(m_tabWidget);
}
