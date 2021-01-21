#include "buildoptiondialog.h"
#include "ui_buildoptiondialog.h"

BuildOptionDialog::BuildOptionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildOptionDialog)
{
    ui->setupUi(this);
    this->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    this->setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
}

BuildOptionDialog::~BuildOptionDialog()
{
    delete ui;
}

void BuildOptionDialog::setEmptyFrame(bool checked)
{
    ui->CB_EmptyFrame->setChecked(checked);
}

void BuildOptionDialog::setEvenOAM(bool checked)
{
    ui->CB_EvenOAM->setChecked(checked);
}

bool BuildOptionDialog::getEmptyFrame()
{
    return ui->CB_EmptyFrame->isChecked();
}

bool BuildOptionDialog::getEvenOAM()
{
    return ui->CB_EvenOAM->isChecked();
}
