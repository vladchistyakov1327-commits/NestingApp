#include "PerformedForm.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>

PerformedForm::PerformedForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadOperations();
}

void PerformedForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить операцию", this);
    m_deleteBtn = new QPushButton("Удалить операцию", this);
    m_saveBtn = new QPushButton("Сохранить", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &PerformedForm::addOperation);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PerformedForm::deleteOperation);
    connect(m_saveBtn, &QPushButton::clicked, this, &PerformedForm::saveOperations);
    
    setLayout(layout);
}

void PerformedForm::loadOperations() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("performed");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить операции");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Дата");
    m_model->setHeaderData(2, Qt::Horizontal, "Сотрудник ID");
    m_model->setHeaderData(3, Qt::Horizontal, "Операция");
    m_model->setHeaderData(4, Qt::Horizontal, "Факт, руб");
    m_model->setHeaderData(5, Qt::Horizontal, "Контроль, руб");
    
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void PerformedForm::addOperation() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void PerformedForm::deleteOperation() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void PerformedForm::saveOperations() {
    if (m_model->submitAll()) {
        QMessageBox::information(this, "Успех", "Операции сохранены");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить: " + m_model->lastError().text());
    }
}
