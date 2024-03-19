#include "mainwindow.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    // Per https://doc.qt.io/qt-5/qregularexpression.html#debugging-code-that-uses-qregularexpression
    // next 2 lines (may be) required to allow `valgrind` to be run on this code with uses `QRegularExpression`
    // alternatively ensure Qt Creator's Tools > Options > Analyzer > Valgrind > Detect self-modifying code is set to Everywhere
//    static char envvar[] = "QT_ENABLE_REGEXP_JIT=0";
//    if (!getenv("QT_ENABLE_REGEXP_JIT"))
//        putenv(envvar);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
