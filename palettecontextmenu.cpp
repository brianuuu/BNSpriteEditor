#include "palettecontextmenu.h"

PaletteContextMenu::PaletteContextMenu(QWidget *parent) : QMenu(parent)
{
    copyColorAction = new QAction("Copy Color", this);
    replaceColorAction = new QAction("Replace Color", this);
    copyPaletteAction = new QAction("Copy Palette", this);
    replacePaletteAction = new QAction("Replace Palette", this);
    insertAboveAction = new QAction("Insert Above", this);
    insertBelowAction = new QAction("Insert Below", this);
    deletePaletteAction = new QAction("Delete Palette", this);
    copyPageAction = new QAction("Copy Page to Clipboard", this);

    this->addAction(copyColorAction);
    this->addAction(replaceColorAction);
    this->addSeparator();
    this->addAction(copyPaletteAction);
    this->addAction(replacePaletteAction);
    this->addAction(insertAboveAction);
    this->addAction(insertBelowAction);
    this->addAction(deletePaletteAction);
    this->addSeparator();
    this->addAction(copyPageAction);

    reset();
}

void PaletteContextMenu::reset()
{
    replaceColorAction->setEnabled(false);
    replacePaletteAction->setEnabled(false);
    insertAboveAction->setEnabled(false);
    insertBelowAction->setEnabled(false);

    m_colorIndex = -1;
    m_paletteIndex = -1;
    m_colorCopied = 0;
    m_paletteCopied.clear();
    m_colorPressed = 0;
    m_palettePressed.clear();
}

void PaletteContextMenu::setColorCopied()
{
    // Copy, only enable replace if copied color is not transparent
    m_colorCopied = m_colorPressed |= 0xFF000000;
    replaceColorAction->setEnabled((m_colorPressed >> 24) == 0xFF);

    QImage image(c_colorSize, c_colorSize, QImage::Format_RGB888);
    image.fill(QColor(m_colorCopied));
    copyImageToClipboard(image);
}

void PaletteContextMenu::setPaletteCopied()
{
    // Copy, undo transparency on first color
    Q_ASSERT(m_palettePressed.size() > 0);
    m_paletteCopied = m_palettePressed;
    m_paletteCopied[0] |= 0xFF000000;

    replacePaletteAction->setEnabled(true);
    insertAboveAction->setEnabled(true);
    insertBelowAction->setEnabled(true);

    QImage image = getSinglePaletteImage(m_paletteCopied);
    copyImageToClipboard(image);
}

void PaletteContextMenu::copyPageToClipboard(PaletteGroup const& group)
{
    if (group.isEmpty()) return;

    if (group[0].size() == 256)
    {
        QImage image = getSinglePaletteImage(group[0]);
        copyImageToClipboard(image);
    }
    else
    {
        QImage output(c_colorSize * 16, c_colorSize * group.size(), QImage::Format_RGBA8888);
        output.fill(0);

        QPainter painter(&output);
        for (int i = 0; i < group.size(); i++)
        {
            Palette const& palette = group[i];
            QImage image = getSinglePaletteImage(palette);
            painter.drawImage(0, i * c_colorSize, image);
        }
        copyImageToClipboard(output);
    }
}

QImage PaletteContextMenu::getSinglePaletteImage(Palette palette)
{
    QImage image(c_colorSize * 16, c_colorSize * (palette.size() / 16), QImage::Format_Indexed8);
    palette[0] |= 0xFF000000; // undo transparency
    image.setColorTable(palette);
    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            image.setPixel(x, y, 16 * (y / c_colorSize) + x / c_colorSize);
        }
    }
    return image;
}

void PaletteContextMenu::copyImageToClipboard(const QImage &image)
{
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    QMimeData* mime = new QMimeData();
    mime->setImageData(image);
    mime->setData("PNG", bArray);
    mime->setData("image/png", bArray);
    qApp->clipboard()->setMimeData(mime);
}
