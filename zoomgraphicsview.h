#ifndef ZOOMGRAPHICSVIEW_H
#define ZOOMGRAPHICSVIEW_H

#include <QApplication>
#include <QDebug>
#include <QGraphicsView>
#include <QScrollBar>
#include <QTimeLine>
#include <QWheelEvent>

class ZoomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ZoomGraphicsView(QWidget *parent = nullptr);

    void wheelEvent(QWheelEvent *event) override;

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void previewScreenPressed(QPoint pos);

public slots:
    void scalingTime(double x);
    void animFinished();

private:
    QPoint pixelPosFromScreen(QPoint pos);

private:
    int m_numScheduledScalings;
    double m_currentScale;

    QPoint m_lastMousePos;
};

#endif // ZOOMGRAPHICSVIEW_H
