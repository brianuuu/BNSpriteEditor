#ifndef PALETTEINFOWINDOW_H
#define PALETTEINFOWINDOW_H

#include <QMainWindow>

namespace Ui {
class PaletteInfoWindow;
}

class PaletteInfoWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PaletteInfoWindow(QWidget *parent = nullptr);
    ~PaletteInfoWindow();

    void setColor(QColor color, int paletteIndex, int colorIndex);

private:
    Ui::PaletteInfoWindow *ui;
};

#endif // PALETTEINFOWINDOW_H
