#include "SalaryForm.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

SalaryForm::SalaryForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadSalary();
}

void SalaryForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить запись", this);
    m_deleteBtn = new QPushButton("Удалить запись", this);
    m_saveBtn = new QPushButton("Сохранить", this);
    m_paidBtn = new QPushButton("Отметить как выплачено", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_paidBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &SalaryForm::addSalary);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SalaryForm::deleteSalary);
    connect(m_saveBtn, &QPushButton::clicked, this, &SalaryForm::saveSalary);
    connect(m_paidBtn, &QPushButton::clicked, this, &SalaryForm::markAsPaid);
    
    setLayout(layout);
}

void SalaryForm::loadSalary() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("salary");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить зарплату");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Сотрудник ID");
    m_model->setHeaderData(2, Qt::Horizontal, "Месяц");
    m_model->setHeaderData(3, Qt::Horizontal, "План, руб");
    m_model->setHeaderData(4, Qt::Horizontal, "Факт, руб");
    m_model->setHeaderData(5, Qt::Horizontal, "Выплачено");
    m_model->setHeaderData(6, Qt::Horizontal, "Разница, руб");
    
    m_tableView->setModel(m_model);
    
    // conditional formatting: highlight rows with large differences in red
    // (to be implemented with custom delegate if needed)
    
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void SalaryForm::addSalary() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void SalaryForm::deleteSalary() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void SalaryForm::saveSalary() {
    // before saving, recalculate differences
    for (int row = 0; row < m_model->rowCount(); ++row) {
        double planned = m_model->data(m_model->index(row, 3)).toDouble();
        double actual = m_model->data(m_model->index(row, 4)).toDouble();
        double diff = actual - planned;
        m_model->setData(m_model->index(row, 6), diff);
    }
    
    if (m_model->submitAll()) {
        QMessageBox::information(this, "Успех", "Зарплата сохранена");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить: " + m_model->lastError().text());
    }
}

void SalaryForm::markAsPaid() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку");
        return;
    }
    
    m_model->setData(m_model->index(index.row(), 5), 1); // 1 = paid
    if (m_model->submitAll()) {
        QMessageBox::information(this, "Успех", "Отмечено как выплачено");
    }
}

void SalaryForm::calculateDifference() {
    // called automatically when saving
}
