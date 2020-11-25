#include "bnspriteeditor.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    BNSpriteEditor w;
    w.show();
    return a.exec();
}
