#ifndef CUSTOMSPRITEIMPORTER_H
#define CUSTOMSPRITEIMPORTER_H

#include <QMainWindow>

namespace Ui {
class CustomSpriteImporter;
}

class CustomSpriteImporter : public QMainWindow
{
    Q_OBJECT

public:
    explicit CustomSpriteImporter(QWidget *parent = nullptr);
    ~CustomSpriteImporter();

    void SetDefaultPath(QString _path) {m_path = _path;}

private:
    Ui::CustomSpriteImporter *ui;

    QString m_path;
};

#endif // CUSTOMSPRITEIMPORTER_H
