#include "paletteinfowindow.h"
#include "ui_paletteinfowindow.h"

PaletteInfoWindow::PaletteInfoWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PaletteInfoWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::CustomizeWindowHint);
}

PaletteInfoWindow::~PaletteInfoWindow()
{
    delete ui;
}

void PaletteInfoWindow::setColor(QColor color, int index)
{
    ui->PaletteWidget->setColor(color, index);
}
