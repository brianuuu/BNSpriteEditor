#include "bnspriteeditor.h"
#include <string>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    BNSpriteEditor w;
    w.show();

    if (argc > 1)
    {
        int argc2;
        LPWSTR* argv2 = CommandLineToArgvW(GetCommandLineW(), &argc2);
        w.passArgument(QString::fromStdWString(std::wstring(argv2[1])), false);
    }

    return a.exec();
}
