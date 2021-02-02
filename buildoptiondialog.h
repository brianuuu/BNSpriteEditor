#ifndef BUILDOPTIONDIALOG_H
#define BUILDOPTIONDIALOG_H

#include <QDialog>
#include <QLayout>

namespace Ui {
class BuildOptionDialog;
}

class BuildOptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BuildOptionDialog(QWidget *parent = nullptr);
    ~BuildOptionDialog();

    void setEmptyFrame(bool checked);
    void setEvenOAM(bool checked);
    void setEvenOAMEnabled(bool enabled);

    bool getEmptyFrame();
    bool getEvenOAM();

    bool getAccepted() { return m_accepted; }

private slots:
    void on_buttonBox_accepted() { m_accepted = true; }
    void on_buttonBox_rejected() { m_accepted = false; }

private:
    Ui::BuildOptionDialog *ui;
    bool m_accepted;
};

#endif // BUILDOPTIONDIALOG_H
