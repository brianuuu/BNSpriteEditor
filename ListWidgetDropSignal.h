#ifndef LISTWIDGETDROPSIGNAL_H
#define LISTWIDGETDROPSIGNAL_H

#include <QDropEvent>
#include <QListWidget>

class ListWidgetDropSignal : public QListWidget
{
    Q_OBJECT
public:
    ListWidgetDropSignal(QWidget * parent) : QListWidget(parent), m_acceptDrop(true), m_acceptFromSource(this) {}
    void SetAcceptDropFromOthers(bool accept) { m_acceptDrop = accept; }
    void SetAcceptFromSource(QWidget* widget) { m_acceptFromSource = widget; }

signals:
    void dropEventSignal(bool isSelf);

protected:
    void dragMoveEvent(QDragMoveEvent *e)
    {
        if (e->source() == this || (m_acceptDrop && e->source() == m_acceptFromSource))
        {
            QListWidget::dragMoveEvent(e);
            e->accept();
        }
        else
        {
            e->ignore();
        }
    }

    void dropEvent(QDropEvent *event)
    {
        QListWidget::dropEvent(event);
        event->accept();
        emit dropEventSignal(event->source() == this);
    }

private:
    bool m_acceptDrop;
    QWidget* m_acceptFromSource;
};

#endif // LISTWIDGETDROPSIGNAL_H
