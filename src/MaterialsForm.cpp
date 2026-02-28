#include "MaterialsForm.h"
#include "MaterialDelegate.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>

MaterialsForm::MaterialsForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadMaterials();
}

void MaterialsForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    // table view
    m_tableView = new QTableView(this);
    layout->addWidget(m_tableView);
    
    // buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Добавить материал", this);
    m_deleteBtn = new QPushButton("Удалить материал", this);
    m_saveBtn = new QPushButton("Сохранить", this);
    
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addBtn, &QPushButton::clicked, this, &MaterialsForm::addMaterial);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MaterialsForm::deleteMaterial);
    connect(m_saveBtn, &QPushButton::clicked, this, &MaterialsForm::saveMaterial);
    
    setLayout(layout);
}

void MaterialsForm::loadMaterials() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("materials");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить материалы");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    // set headers
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Наименование");
    m_model->setHeaderData(2, Qt::Horizontal, "Ед. изм.");
    m_model->setHeaderData(3, Qt::Horizontal, "Цена, руб");
    m_model->setHeaderData(4, Qt::Horizontal, "Текущий остаток");
    m_model->setHeaderData(5, Qt::Horizontal, "Мин. остаток");
    m_model->setHeaderData(6, Qt::Horizontal, "Поставщик");
    
    m_tableView->setModel(m_model);
    
    // apply custom delegate for highlighting low stock
    MaterialDelegate *delegate = new MaterialDelegate(this);
    m_tableView->setItemDelegate(delegate);
    
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}

void MaterialsForm::addMaterial() {
    int row = m_model->rowCount();
    m_model->insertRow(row);
    m_tableView->scrollToBottom();
}

void MaterialsForm::deleteMaterial() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Ошибка", "Выберите строку для удаления");
        return;
    }
    m_model->removeRow(index.row());
}

void MaterialsForm::saveMaterial() {
    if (m_model->submitAll()) {
        QMessageBox::information(this, "Успех", "Материалы сохранены");
    } else {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось сохранить: ") + m_model->lastError().text());
    }
}
