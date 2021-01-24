#ifndef PALETTECONTEXTMENU_H
#define PALETTECONTEXTMENU_H

#include <QMenu>

typedef QVector<QRgb> Palette;

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

    void setColorPressed(QRgb color) { m_colorPressed = color; }
    void setPalettePressed(Palette palette) { m_palettePressed = palette; }

    void setColorCopied();
    void setPaletteCopied();
    QRgb getColorcopied() const { return m_colorCopied; }
    Palette getPalettecopied() const { return m_paletteCopied; }

public:
    QAction* copyColorAction;
    QAction* replaceColorAction;

    QAction* copyPaletteAction;
    QAction* replacePaletteAction;
    QAction* insertAboveAction;
    QAction* insertBelowAction;
    QAction* deletePaletteAction;

private:
    int m_colorIndex;
    int m_paletteIndex;

    QRgb m_colorCopied;
    Palette m_paletteCopied;

    QRgb m_colorPressed;
    Palette m_palettePressed;
};

#endif // PALETTECONTEXTMENU_H
