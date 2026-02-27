#include "ShipmentForm.h"
#include "InventoryManager.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

ShipmentForm::ShipmentForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadShipments();
}

void ShipmentForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить отгрузку", this);
    m_deleteBtn = new QPushButton("Удалить отгрузку", this);
    m_shipBtn = new QPushButton("Отгрузить (списать материалы)", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_shipBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &ShipmentForm::addShipment);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ShipmentForm::deleteShipment);
    connect(m_shipBtn, &QPushButton::clicked, this, &ShipmentForm::shipNow);
    
    setLayout(layout);
}

void ShipmentForm::loadShipments() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("shipments");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить отгрузки");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Дата");
    m_model->setHeaderData(2, Qt::Horizontal, "Изделие");
    m_model->setHeaderData(3, Qt::Horizontal, "Количество");
    m_model->setHeaderData(4, Qt::Horizontal, "Заказчик");
    m_model->setHeaderData(5, Qt::Horizontal, "Статус");
    
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void ShipmentForm::addShipment() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void ShipmentForm::deleteShipment() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void ShipmentForm::shipNow() {
    // save current edits first
    if (!m_model->submitAll()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить отгрузки");
        return;
    }
    
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите отгрузку для обработки");
        return;
    }
    
    // get product and quantity from current row
    QString product = m_model->data(m_model->index(idx.row(), 2)).toString();
    double qty = m_model->data(m_model->index(idx.row(), 3)).toDouble();
    QString orderId = m_model->data(m_model->index(idx.row(), 0)).toString();
    
    // call automatic write-off
    if (InventoryManager::writeOffForShipment(product, qty, orderId)) {
        // mark shipment as processed
        m_model->setData(m_model->index(idx.row(), 5), "Отгружено");
        if (m_model->submitAll()) {
            QMessageBox::information(this, "Успех", 
                QString("Отгружено %1 шт. изделия '%2'. Материалы списаны автоматически.")
                    .arg(qty).arg(product));
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось списать материалы");
    }
}
