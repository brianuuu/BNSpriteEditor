#include "paletteinfowidget.h"
#include "ui_paletteinfowidget.h"

PaletteInfoWidget::PaletteInfoWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaletteInfoWidget)
{
    ui->setupUi(this);

    m_graphicsScene = new QGraphicsScene(this);
    ui->Color_GV->setScene(m_graphicsScene);
}

PaletteInfoWidget::~PaletteInfoWidget()
{
    delete ui;
}

void PaletteInfoWidget::setColor(QColor color, int index)
{
    ui->Palette_Index->setText(QString::number(index));

    ui->Color_R->setText(QString::number(color.red()));
    ui->Color_G->setText(QString::number(color.green()));
    ui->Color_B->setText(QString::number(color.blue()));

    int hex = BNSprite::RGBtoGBA(color.rgb());
    ui->Color_Hex->setText("0x" + QString("%1").arg(hex, 4, 16, QChar('0')).toUpper());

    ui->Color_GV->setBackgroundBrush(QBrush(color));
}
