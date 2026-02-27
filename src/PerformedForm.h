#ifndef PERFORMEDFORM_H
#define PERFORMEDFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>

class PerformedForm : public QWidget {
    Q_OBJECT
public:
    PerformedForm(QWidget *parent = nullptr);

private slots:
    void addOperation();
    void deleteOperation();
    void saveOperations();

private:
    void setupUI();
    void loadOperations();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_saveBtn;
    QSqlTableModel *m_model;
};

#endif // PERFORMEDFORM_H
