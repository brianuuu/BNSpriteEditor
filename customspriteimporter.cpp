#include "customspriteimporter.h"
#include "ui_customspriteimporter.h"

CustomSpriteImporter::CustomSpriteImporter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CustomSpriteImporter)
{
    ui->setupUi(this);
}

CustomSpriteImporter::~CustomSpriteImporter()
{
    delete ui;
}
