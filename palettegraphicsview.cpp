#include "palettegraphicsview.h"

PaletteGraphicsView::PaletteGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    m_graphicsScene = new QGraphicsScene(this);
    this->setScene(m_graphicsScene);
    this->setMouseTracking(true);
    this->setViewportUpdateMode(ViewportUpdateMode::FullViewportUpdate);
    this->setAlignment(Qt::AlignTop | Qt::AlignTop);

    QBrush brush(QColor(240,240,240));
    this->setBackgroundBrush(brush);

    QImage image = QImage((c_size - 1) * 16 + 1, c_size, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            if (x <= 1 || x >= image.width() - 2 || y <= 1 || y >= image.height() - 2)
            {
                image.setPixel(x, y, qRgb(255, 0, 0));
            }
            else
            {
                image.setPixel(x, y, 0);
            }
        }
    }
    m_highlight = m_graphicsScene->addPixmap(QPixmap::fromImage(image));
    m_highlight->setZValue(1000);
    m_highlight->setVisible(false);

    m_index = -1;
    m_mouseEventEnabled = true;
    m_mousePos = QPoint(0,0);

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(updateInfo()));
    m_infoWindow = new PaletteInfoWindow();
    m_infoWindow->setAttribute(Qt::WA_ShowWithoutActivating);
}

PaletteGraphicsView::~PaletteGraphicsView()
{
    clear();
}

void PaletteGraphicsView::addPalette(Palette palette, int insertAt)
{
    // Make sure size is 16
    while (palette.size() > 16)
    {
        palette.pop_back();
    }
    while (palette.size() < 16)
    {
        palette.push_back(0xFF000000);
    }
    palette.push_back(c_unselected);    // 16

    QImage* image = new QImage(c_width, c_size, QImage::Format_Indexed8);
    image->setColorTable(palette);

    for (int y = 0; y < image->height(); y++)
    {
        for (int x = 0; x < image->width(); x++)
        {
            if (x == 0 || x % (c_size - 1) == 0 || y == 0 || y == image->height() - 1)
            {
                image->setPixel(x, y, palette.size() - 1);
            }
            else
            {
                image->setPixel(x, y, x / (c_size - 1));
            }
        }
    }
    if (insertAt == -1 || insertAt > m_images.size())
    {
        m_images.push_back(image);
    }
    else
    {
        m_images.insert(insertAt, image);
    }

    QGraphicsPixmapItem* item = m_graphicsScene->addPixmap(QPixmap::fromImage(*image));
    if (insertAt == -1 || insertAt > m_pixmapItems.size())
    {
        m_pixmapItems.push_back(item);
        item->setPos(0, (m_images.size() - 1) * c_size);
    }
    else
    {
        m_pixmapItems.insert(insertAt, item);
        item->setPos(0, insertAt * c_size);

        // Shift everything after this index down
        for (int i = insertAt + 1 ; i < m_pixmapItems.size(); i++)
        {
            QGraphicsPixmapItem* t = m_pixmapItems[i];
            t->moveBy(0, c_size);
        }
    }

    m_graphicsScene->setSceneRect(0, 0, c_width, m_images.size() * 16);
}

void PaletteGraphicsView::setPaletteSelected(int index)
{
    Q_ASSERT(index >= 0 && index < m_images.size());

    if (index >= 0 && index < m_images.size())
    {
        m_index = index;
        m_highlight->setVisible(true);
        m_highlight->setPos(0, index * c_size);

        // Scroll to palette if it is off-screen
        QScrollBar* vScrollBar = this->verticalScrollBar();
        int height = this->size().height() - 2;
        int yScroll = vScrollBar->value();
        int selectedTopY = index * 16;
        int selectedBaseY = (index + 1) * 16;
        if (selectedBaseY > height + yScroll)
        {
            vScrollBar->setValue(selectedBaseY - height);
        }
        else if (selectedTopY < yScroll)
        {
            vScrollBar->setValue(selectedTopY);
        }
    }
}

void PaletteGraphicsView::deletePalette(int index)
{
    Q_ASSERT(index >= 0 && index < m_images.size());

    if (index >= 0 && index < m_images.size())
    {
        delete m_images[index];
        m_images.erase(m_images.begin() + index);

        QGraphicsPixmapItem* item = m_pixmapItems[index];
        m_graphicsScene->removeItem(item);
        delete item;
        m_pixmapItems.erase(m_pixmapItems.begin() + index);

        m_graphicsScene->setSceneRect(0, 0, c_width, m_images.size() * 16);

        // Shift everything after this index up
        for (int i = index ; i < m_pixmapItems.size(); i++)
        {
            QGraphicsPixmapItem* item = m_pixmapItems[i];
            item->moveBy(0, -c_size);
        }

        // Highlight is outside limit, move it up
        if (m_index == m_pixmapItems.size())
        {
            m_index--;
            m_highlight->moveBy(0, -c_size);
        }
    }
}

void PaletteGraphicsView::clear()
{
    for (QImage* image : m_images)
    {
        delete image;
    }
    m_images.clear();

    m_index = -1;

    for (QGraphicsPixmapItem* item : m_pixmapItems)
    {
        m_graphicsScene->removeItem(item);
        delete item;
    }
    m_pixmapItems.clear();
    m_graphicsScene->setSceneRect(0, 0, 0, 0);
    m_highlight->setVisible(false);
}

void PaletteGraphicsView::replaceColor(int paletteIndex, int colorIndex, QRgb color)
{
    QImage* image = m_images[paletteIndex];
    Palette palette = image->colorTable();

    // Update color (covert it back and forth to round for GBA color)
    color = BNSprite::ClampRGB(color);
    palette[colorIndex] = color;

    image->setColorTable(palette);
    m_pixmapItems[paletteIndex]->setPixmap(QPixmap::fromImage(*image));
}

void PaletteGraphicsView::replacePalette(int paletteIndex, Palette palette)
{
    if (paletteIndex >= m_images.size()) return;

    // Make sure size is 16
    while (palette.size() > 16)
    {
        palette.pop_back();
    }
    while (palette.size() < 16)
    {
        palette.push_back(0xFF000000);
    }
    palette.push_back(c_unselected);    // 16

    QImage* image = m_images[paletteIndex];
    image->setColorTable(palette);

    QGraphicsPixmapItem* item = m_pixmapItems[paletteIndex];
    item->setPixmap(QPixmap::fromImage(*image));
}

void PaletteGraphicsView::swapPalette(int id1, int id2)
{
    qSwap(m_images[id1], m_images[id2]);
    qSwap(m_pixmapItems[id1], m_pixmapItems[id2]);

    QPointF tempPos = m_pixmapItems[id1]->pos();
    m_pixmapItems[id1]->setPos(m_pixmapItems[id2]->pos());
    m_pixmapItems[id2]->setPos(tempPos);
}

void PaletteGraphicsView::closeInfo()
{
    m_infoWindow->close();
    m_mousePos = QPoint(0,0);
}

QPoint PaletteGraphicsView::getPixelPos(QPoint pos)
{
    return pos + QPoint(this->horizontalScrollBar()->value(), this->verticalScrollBar()->value());
}

void PaletteGraphicsView::enterEvent(QEvent *event)
{
    m_timer->start(10);
}

void PaletteGraphicsView::leaveEvent(QEvent *event)
{
    m_timer->stop();
    closeInfo();
}

void PaletteGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (m_mouseEventEnabled)
    {
        QPoint pos = getPixelPos(m_mousePos);
        if (pos.x() != 0 && pos.x() <= (c_size - 1) * 16 && pos.y() < m_images.size() * c_size)
        {
            int paletteIndex = pos.y() / c_size;
            int colorIndex = (pos.x() - 1) / (c_size - 1);

            if (event->button() == Qt::LeftButton)
            {
                QImage const* image = m_images[paletteIndex];
                Palette const palette = image->colorTable();
                QRgb const color = palette[colorIndex];
                QColor const newColor = QColorDialog::getColor(QColor(color), this, "Pick a Color");
                if (newColor.isValid())
                {
                    replaceColor(paletteIndex, colorIndex, newColor.rgb());
                    emit colorChanged(paletteIndex, colorIndex, color);
                }
            }
            else if (event->button() == Qt::RightButton)
            {
                emit paletteContextMenuRequested(paletteIndex, colorIndex, this->mapToGlobal(event->pos()));
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void PaletteGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    updateInfo();
    QGraphicsView::mouseMoveEvent(event);
}

void PaletteGraphicsView::updateInfo()
{
    if (this->verticalScrollBar()->mapFromGlobal(QCursor::pos()).x() >= 0)
    {
        closeInfo();
        return;
    }

    QPoint pos = getPixelPos(m_mousePos);
    if (pos.x() != 0 && pos.x() <= (c_size - 1) * 16 && pos.y() < m_images.size() * c_size)
    {
        int paletteIndex = pos.y() / c_size;
        int colorIndex = (pos.x() - 1) / (c_size - 1);

        if (paletteIndex >= m_images.size() || colorIndex > 15)
        {
            closeInfo();
            return;
        }

        QImage* image = m_images[paletteIndex];
        Palette const palette = image->colorTable();
        QColor color = palette[colorIndex];

        m_infoWindow->setColor(color, paletteIndex);
        m_infoWindow->show();
        m_infoWindow->raise();
        m_infoWindow->move(QCursor::pos() + QPoint(2,2));
    }
    else
    {
        closeInfo();
    }
}
