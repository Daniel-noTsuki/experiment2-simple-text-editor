#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("简单文本编辑器"));
    QApplication::setOrganizationName(QStringLiteral("DGUT"));

    MainWindow window;
    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--self-test"))) {
        QTextStream out(stdout);
        const bool ok = window.runSelfTests(out);
        return ok ? 0 : 1;
    }

    window.show();
    const int captureIndex = args.indexOf(QStringLiteral("--capture"));
    if (captureIndex >= 0 && captureIndex + 1 < args.size()) {
        const QString outputDir = QDir(args.at(captureIndex + 1)).absolutePath();
        QTimer::singleShot(300, &window, [&window, outputDir]() {
            window.captureEvidence(outputDir);
            qApp->quit();
        });
    }
    return app.exec();
}

