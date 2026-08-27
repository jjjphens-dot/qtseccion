#ifndef RESULTINPUTDIALOG_H
#define RESULTINPUTDIALOG_H

#include <QDialog>
#include "csportman.h"

namespace Ui {
class ResultInputDialog;
}

class ResultInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResultInputDialog(QWidget *parent = nullptr);
    ~ResultInputDialog();

    void SetAthleteList(const QVector<CSportMan> &athletes);
    void SelectAthlete(int index);
    int  GetSelectedAthleteIndex() const;
    void GetResults(float results[5]) const;

private slots:
    void onAthleteChanged(int index);

private:
    Ui::ResultInputDialog *ui;
    QVector<CSportMan> m_athletes;
};

#endif // RESULTINPUTDIALOG_H