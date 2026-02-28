#include "EmployeesForm.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>

EmployeesForm::EmployeesForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadEmployees();
}

void EmployeesForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить сотрудника", this);
    m_deleteBtn = new QPushButton("Удалить сотрудника", this);
    m_saveBtn = new QPushButton("Сохранить", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &EmployeesForm::addEmployee);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EmployeesForm::deleteEmployee);
    connect(m_saveBtn, &QPushButton::clicked, this, &EmployeesForm::saveEmployees);
    
    setLayout(layout);
}

void EmployeesForm::loadEmployees() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("employees");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить сотрудников");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "ФИО");
    m_model->setHeaderData(2, Qt::Horizontal, "Должность");
    m_model->setHeaderData(3, Qt::Horizontal, "Дата рождения");
    m_model->setHeaderData(4, Qt::Horizontal, "Дата найма");
    m_model->setHeaderData(5, Qt::Horizontal, "Месячный план, руб");
    m_model->setHeaderData(6, Qt::Horizontal, "Тип зарплаты");
    
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void EmployeesForm::addEmployee() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void EmployeesForm::deleteEmployee() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void EmployeesForm::saveEmployees() {
    if (m_model->submitAll()) {
        QMessageBox::information(this, "Успех", "Сотрудники сохранены");
    } else {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось сохранить: ") + m_model->lastError().text());
    }
}
