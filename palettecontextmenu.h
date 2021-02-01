#ifndef PALETTECONTEXTMENU_H
#define PALETTECONTEXTMENU_H

#include <QBuffer>
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

typedef QVector<QRgb> Palette;
typedef QVector<Palette> PaletteGroup;

class PaletteContextMenu : public QMenu
{
    Q_OBJECT
public:
    PaletteContextMenu(QWidget *parent = nullptr);

    void reset();

    void setColorIndex(int index) { m_colorIndex = index; }
    void setPaletteIndex(int index) { m_paletteIndex = index; }
    int getColorIndex() const { return m_colorIndex; }
    int getPaletteIndex() const { return m_paletteIndex; }

    void setColorPressed(QRgb const& color) { m_colorPressed = color; }
    void setPalettePressed(Palette const& palette) { m_palettePressed = palette; }

    void setColorCopied();
    void setPaletteCopied();
    QRgb getColorcopied() const { return m_colorCopied; }
    Palette getPalettecopied() const { return m_paletteCopied; }

    void copyPageToClipboard(PaletteGroup const& group);

public:
    QAction* copyColorAction;
    QAction* replaceColorAction;

    QAction* copyPaletteAction;
    QAction* replacePaletteAction;
    QAction* insertAboveAction;
    QAction* insertBelowAction;
    QAction* deletePaletteAction;

    QAction* copyPageAction;

private:
    QImage getSinglePaletteImage(Palette palette);
    void copyImageToClipboard(QImage const& image);

    int m_colorIndex;
    int m_paletteIndex;

    QRgb m_colorCopied;
    Palette m_paletteCopied;
    static const int c_colorSize = 8;

    QRgb m_colorPressed;
    Palette m_palettePressed;
};

#endif // PALETTECONTEXTMENU_H
