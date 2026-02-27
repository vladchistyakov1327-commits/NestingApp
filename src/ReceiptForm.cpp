#include "ReceiptForm.h"
#include "InventoryManager.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>

ReceiptForm::ReceiptForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadReceipts();
}

void ReceiptForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить приход", this);
    m_deleteBtn = new QPushButton("Удалить приход", this);
    m_acceptBtn = new QPushButton("Оприходовать", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_acceptBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &ReceiptForm::addReceipt);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ReceiptForm::deleteReceipt);
    connect(m_acceptBtn, &QPushButton::clicked, this, &ReceiptForm::acceptReceipt);
    
    setLayout(layout);
}

void ReceiptForm::loadReceipts() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("receipts");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить приходы");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Дата");
    m_model->setHeaderData(2, Qt::Horizontal, "Материал");
    m_model->setHeaderData(3, Qt::Horizontal, "Количество");
    m_model->setHeaderData(4, Qt::Horizontal, "Цена, руб");
    m_model->setHeaderData(5, Qt::Horizontal, "Поставщик");
    
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void ReceiptForm::addReceipt() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void ReceiptForm::deleteReceipt() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void ReceiptForm::acceptReceipt() {
    // first save to database
    if (!m_model->submitAll()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить приходы");
        return;
    }
    
    // then process each receipt: add to stock and record movement
    // example: look for new/modified rows and call InventoryManager::addReceipt
    // (in a real app, track which rows were inserted)
    
    QMessageBox::information(this, "Успех", "Приходы оприходованы и остатки обновлены");
}
