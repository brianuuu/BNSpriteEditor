#ifndef LISTWIDGETIGNORESELF_H
#define LISTWIDGETIGNORESELF_H

#include <QDebug>
#include <QDragMoveEvent>
#include <QListWidget>

class ListWidgetIgnoreSelf : public QListWidget
{
    Q_OBJECT
public:
    ListWidgetIgnoreSelf(QWidget * parent) : QListWidget(parent) {}

signals:
    void keyUpDownSignal(QListWidgetItem* item);

protected:
    void dragMoveEvent(QDragMoveEvent *e)
    {
        e->ignore();
    }

    void keyPressEvent(QKeyEvent* e)
    {
        QListWidget::keyPressEvent(e);
        if (this->hasFocus())
        {
            if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
            {
                emit keyUpDownSignal(Q_NULLPTR);
            }
        }
    }
};

#endif // LISTWIDGETIGNORESELF_H
