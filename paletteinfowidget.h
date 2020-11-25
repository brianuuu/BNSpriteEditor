#ifndef PALETTEINFOWIDGET_H
#define PALETTEINFOWIDGET_H

#include <QGraphicsScene>
#include <QWidget>
#include "bnsprite.h"

namespace Ui {
class PaletteInfoWidget;
}

class PaletteInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PaletteInfoWidget(QWidget *parent = nullptr);
    ~PaletteInfoWidget();

    void setColor(QColor color, int index);

private:
    Ui::PaletteInfoWidget *ui;

    QGraphicsScene* m_graphicsScene;
};

#endif // PALETTEINFOWIDGET_H
