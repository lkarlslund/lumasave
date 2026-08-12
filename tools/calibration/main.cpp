// SPDX-License-Identifier: MIT
#include "calibrationcontroller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("LumaSave"));
    app.setApplicationName(QStringLiteral("Calibration"));
    CalibrationController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("calibrationBackend"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/calibration/main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, &CalibrationController::restore);
    return app.exec();
}
