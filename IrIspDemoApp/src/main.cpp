/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IR Image Processing Studio"));
    QApplication::setOrganizationName(QStringLiteral("Wuhan Wavefront Technology Co., Ltd."));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/brand/icon.png")));
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion")))
        QApplication::setStyle(QStringLiteral("Fusion"));

    MainWindow w;
    w.show();
    return app.exec();
}
