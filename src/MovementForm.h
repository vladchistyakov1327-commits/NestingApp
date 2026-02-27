#ifndef MOVEMENTFORM_H
#define MOVEMENTFORM_H

#include <QWidget>
#include <QTableView>
#include <QSqlTableModel>

class MovementForm : public QWidget {
    Q_OBJECT
public:
    MovementForm(QWidget *parent = nullptr);

private:
    void setupUI();
    void loadMovements();

    QTableView *m_tableView;
    QSqlTableModel *m_model;
};

#endif // MOVEMENTFORM_H
