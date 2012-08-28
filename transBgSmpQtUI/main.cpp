#include <QtGui/QApplication>
#include <QWSServer>
#include "transBgSmpQtUI.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QWSServer::setBackground(QBrush(QColor(0, 0, 0, 0)));
    Widget w;
    w.setWindowFlags(Qt::FramelessWindowHint);
    w.setAttribute(Qt::WA_TranslucentBackground, true);
    w.show();

    return a.exec();
}
