#ifndef PALETTEGRAPHICSVIEW_H
#define PALETTEGRAPHICSVIEW_H

#include <QColorDialog>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "paletteinfowidget.h"
#include "paletteinfowindow.h"

typedef QVector<QRgb> Palette;

class PaletteGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PaletteGraphicsView(QWidget *parent = nullptr);
    ~PaletteGraphicsView() override;

    void setMouseEventEnabled(bool enabled) { m_mouseEventEnabled = enabled; }
    void addPalette(Palette palette, bool is256Color, int insertAt = -1);
    void setPaletteSelected(int index);
    void deletePalette(int index);
    void clear();

    void replaceColor(int paletteIndex, int colorIndex, QRgb color);
    void replacePalette(int paletteIndex, Palette palette);
    void swapPalette(int id1, int id2);

    void closeInfo();

protected:
    virtual void enterEvent(QEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
    QPoint getPixelPos(QPoint pos);

signals:
    void colorChanged(int paletteIndex, int colorIndex, QRgb color);
    void paletteContextMenuRequested(int paletteIndex, int colorIndex, QPoint pos);

public slots:
    void updateInfo();

private:
    int const c_size = 16;
    QRgb const c_unselected = qRgb(192,192,192);
    QRgb const c_selected = qRgb(255,0,0);
    int const c_width = (c_size - 1) * 16 + 1;

    QTimer* m_timer;
    PaletteInfoWindow* m_infoWindow;

    int m_index;
    bool m_mouseEventEnabled;
    QPoint m_mousePos;
    QGraphicsScene* m_graphicsScene;
    QGraphicsPixmapItem* m_highlight;
    QVector<QImage*> m_images;
    QVector<QGraphicsPixmapItem*> m_pixmapItems;
};

#endif // PALETTEGRAPHICSVIEW_H
