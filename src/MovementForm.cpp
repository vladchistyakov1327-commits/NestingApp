#include "MovementForm.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>

MovementForm::MovementForm(QWidget *parent)
    : QWidget(parent), m_model(nullptr) {
    setupUI();
    loadMovements();
}

void MovementForm::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // read-only
    layout->addWidget(m_tableView);
    
    setLayout(layout);
}

void MovementForm::loadMovements() {
    m_model = new QSqlTableModel(this, DatabaseManager::instance().database());
    m_model->setTable("movements");
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    
    if (!m_model->select()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить журнал движения");
        qDebug() << m_model->lastError().text();
        return;
    }
    
    m_model->setHeaderData(0, Qt::Horizontal, "ID");
    m_model->setHeaderData(1, Qt::Horizontal, "Дата");
    m_model->setHeaderData(2, Qt::Horizontal, "Операция");
    m_model->setHeaderData(3, Qt::Horizontal, "Материал");
    m_model->setHeaderData(4, Qt::Horizontal, "Количество");
    m_model->setHeaderData(5, Qt::Horizontal, "Основание");
    
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
}
