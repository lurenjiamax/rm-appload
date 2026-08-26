#include <QGuiApplication>
#include <QDebug>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QString>
#include <qqml.h>

#include "qtfb/FBController.h"
#include "qtfb/fbmanagement.h"

#include "AppLoadCoordinator.h"
#include "Launcher.h"
#include "AppLibrary.h"
#include "AppLoad.h"
#include "management.h"
#include "library.h"
#include "log.h"
#include "xovi.h"

#include "../resources.cpp"

bool qRegisterResourceData(int version, const unsigned char *tree, const unsigned char *name, const unsigned char *data);

static QString readSystemImageVersion() {
    QFile versionFile("/etc/os-release");
    if (!versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QDEBUG << "Unable to read /etc/os-release; QMD hooks will not be loaded.";
        return QString();
    }

    while (!versionFile.atEnd()) {
        QString line = QString::fromUtf8(versionFile.readLine()).trimmed();
        if (!line.startsWith("IMG_VERSION=")) {
            continue;
        }

        QString version = line.mid(QStringLiteral("IMG_VERSION=").size()).trimmed();
        if (version.size() >= 2 && version.startsWith('"') && version.endsWith('"')) {
            version = version.mid(1, version.size() - 2);
        }
        return version;
    }

    QDEBUG << "IMG_VERSION not found in /etc/os-release; QMD hooks will not be loaded.";
    return QString();
}

static bool isXochitl326Or327Version(const QString &version) {
    return version == "3.26.0.68"
        || version.startsWith(QStringLiteral("3.27."));
}

static bool isXochitl328Version(const QString &version) {
    return version.startsWith(QStringLiteral("3.28."));
}

static void addExternalDiff(const char *contents, const char *identifier) {
    if (!qt_resource_rebuilder$qmldiff_add_external_diff(contents, identifier)) {
        QDEBUG << "QMD diff patch was not accepted:" << identifier;
    }
}

static void addVersionedAppLoadDiff() {
    QString systemVersion = readSystemImageVersion();
    QDEBUG << "Detected system image version:" << systemVersion;

    if (isXochitl328Version(systemVersion)) {
        addExternalDiff(r$apploadDiff328, "AppLoad hooks for xochitl 3.28.x");
    } else if (isXochitl326Or327Version(systemVersion)) {
        addExternalDiff(r$apploadDiff326327, "AppLoad hooks for xochitl 3.26/3.27");
    } else {
        QDEBUG << "Unsupported xochitl system version; QMD hooks skipped:" << systemVersion;
    }
}

static void registerAppLoadQmlTypes() {
    qmlRegisterModule("net.asivery.AppLoad", 1, 0);
    qmlRegisterModule("net.asivery.Framebuffer", 1, 0);

    const int appLoadTypeId = qmlRegisterType<AppLoad>("net.asivery.AppLoad", 1, 0, "AppLoad");
    const int coordinatorTypeId = qmlRegisterType<AppLoadCoordinator>("net.asivery.AppLoad", 1, 0, "AppLoadCoordinator");
    const int libraryTypeId = qmlRegisterType<AppLoadLibrary>("net.asivery.AppLoad", 1, 0, "AppLoadLibrary");
    const int applicationTypeId = qmlRegisterType<AppLoadApplication>("net.asivery.AppLoad", 1, 0, "AppLoadApplication");
    const int fbControllerTypeId = qmlRegisterType<FBController>("net.asivery.Framebuffer", 1, 0, "FBController");
    const int launcherTypeId = qmlRegisterSingletonType<AppLoadLauncher>("net.asivery.AppLoad", 1, 0, "AppLoadLauncher", &AppLoadLauncher::qmlSingleton);

    QDEBUG << "Registered QML modules:"
           << "net.asivery.AppLoad"
           << appLoadTypeId
           << coordinatorTypeId
           << libraryTypeId
           << applicationTypeId
           << launcherTypeId
           << "net.asivery.Framebuffer"
           << fbControllerTypeId;
}

extern "C" {
    static const char *applicationRoot;
    void _xovi_construct() {
        applicationRoot = Environment->getExtensionDirectory("appload");

        registerAppLoadQmlTypes();
        addVersionedAppLoadDiff();

        // AppLoad requires qt-resource-rebuilder to edit its own source code once it's being loaded
        // But we're not done initializing the modules! There can be other modules which require qmldiff
        // to add more diffs. Therefore, we request qmldiff to disable slots support temporarily, which
        // will put it in a state where it can edit the QML source code that has not been seen yet, but will
        // also not block adding additional slots to it, using additional externals.
        // Appload's QML itself does not
        qt_resource_rebuilder$qmldiff_disable_slots_while_processing();
        qRegisterResourceData(3, qt_resource_struct, qt_resource_name, qt_resource_data);
        qt_resource_rebuilder$qmldiff_enable_slots_while_processing();

        appload::library::loadApplications();
        qtfb::management::start();
    }
}

const char *getApplicationDirectoryRoot(void) {
    return applicationRoot;
}
