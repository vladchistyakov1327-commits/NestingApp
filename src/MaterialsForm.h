#ifndef MATERIALSFORM_H
#define MATERIALSFORM_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QSqlTableModel>

class MaterialsForm : public QWidget {
    Q_OBJECT
public:
    MaterialsForm(QWidget *parent = nullptr);

private slots:
    void addMaterial();
    void deleteMaterial();
    void saveMaterial();

private:
    void setupUI();
    void loadMaterials();

    QTableView *m_tableView;
    QPushButton *m_addBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_saveBtn;
    QSqlTableModel *m_model;
};

#endif // MATERIALSFORM_H
