// Reference: https://wiki.qt.io/Smooth_Zoom_In_QGraphicsView

#include "zoomgraphicsview.h"

ZoomGraphicsView::ZoomGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    m_numScheduledScalings = 0;
    m_currentScale = 1.0;

    this->setDragMode(QGraphicsView::NoDrag);
    this->setMouseTracking(true);
    m_lastMousePos = QPoint(0,0);
}

void ZoomGraphicsView::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15; // see QWheelEvent documentation
    m_numScheduledScalings += numSteps;
    /*if (m_numScheduledScalings * numSteps < 0)
    {
        // if user moved the wheel in another direction, we reset previously scheduled scalings
        m_numScheduledScalings = numSteps;
    }*/

    QTimeLine *anim = new QTimeLine(350, this);
    anim->setUpdateInterval(10);

    connect(anim, SIGNAL (valueChanged(double)), SLOT (scalingTime(double)));
    connect(anim, SIGNAL (finished()), SLOT (animFinished()));
    anim->start();
}

QPoint ZoomGraphicsView::pixelPosFromScreen(QPoint pos)
{
    // Account for zoom level, find the pixel relative to screen click
    int hValue = this->horizontalScrollBar()->value();
    int vValue = this->verticalScrollBar()->value();
    QPoint pixelPos;
    pixelPos.setX(((double)hValue + (double)pos.x()) / m_currentScale);
    pixelPos.setY(((double)vValue + (double)pos.y()) / m_currentScale);

    return pixelPos;
}

void ZoomGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        QApplication::setOverrideCursor(QCursor(Qt::SizeAllCursor));
        m_lastMousePos = event->pos();
        event->accept();
    }
    else if (event->button() == Qt::LeftButton)
    {
        emit previewScreenPressed(pixelPosFromScreen(event->pos()));
    }

    QGraphicsView::mousePressEvent(event);
}

void ZoomGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    // pan with a middle mouse drag
    if (event->buttons() & Qt::MiddleButton)
    {
        QPoint dPos = event->pos() - m_lastMousePos;
        this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - dPos.x());
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - dPos.y());

        m_lastMousePos = event->pos();
        event->accept();
    }
    else if (event->buttons() & Qt::LeftButton)
    {
        emit previewScreenPressed(pixelPosFromScreen(event->pos()));
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ZoomGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QApplication::restoreOverrideCursor();
}

void ZoomGraphicsView::scalingTime(double x)
{
    double factor = 1.0 + double(m_numScheduledScalings) / 500.0;

    // Min zoom
    if (factor < 1.0 && m_currentScale * factor < 1.001)
    {
        factor = 1.0 / m_currentScale;
        if (factor == 1.0) return;
    }

    // Max zoom
    double maxZoom = 4.0;
    if (factor > 1.0 && m_currentScale * factor > maxZoom)
    {
        factor = maxZoom / m_currentScale;
        if (factor == 1.0) return;
    }

    bool initialScale = (m_currentScale == 1.0);

    m_currentScale *= factor;
    scale(factor, factor);

    if (initialScale)
    {
        QScrollBar* hBar = this->horizontalScrollBar();
        hBar->setValue(hBar->maximum() / 2);
        QScrollBar* vBar = verticalScrollBar();
        vBar->setValue(vBar->maximum() / 2);
    }
}

void ZoomGraphicsView::animFinished()
{
    if (m_numScheduledScalings > 0)
    {
        m_numScheduledScalings--;
    }
    else
    {
        m_numScheduledScalings++;
    }

    sender()->~QObject();
}
