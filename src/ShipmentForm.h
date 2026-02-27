#ifndef SHIPMENTFORM_H
#define SHIPMENTFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>

class ShipmentForm : public QWidget {
    Q_OBJECT
public:
    ShipmentForm(QWidget *parent = nullptr);

private slots:
    void addShipment();
    void deleteShipment();
    void shipNow();

private:
    void setupUI();
    void loadShipments();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_shipBtn;
    QSqlTableModel *m_model;
};

#endif // SHIPMENTFORM_H
