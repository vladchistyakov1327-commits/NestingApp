#ifndef RECEIPTFORM_H
#define RECEIPTFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>
#include <QComboBox>
#include <QSpinBox>

class ReceiptForm : public QWidget {
    Q_OBJECT
public:
    ReceiptForm(QWidget *parent = nullptr);

private slots:
    void addReceipt();
    void deleteReceipt();
    void acceptReceipt();

private:
    void setupUI();
    void loadReceipts();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_acceptBtn;
    QSqlTableModel *m_model;
};

#endif // RECEIPTFORM_H
