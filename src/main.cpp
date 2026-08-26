#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <qqml.h>

#include "AppLoadCoordinator.h"
#include "EmuOnly.h"
#include "Launcher.h"
#include "AppLibrary.h"
#include "AppLoad.h"
#include "management.h"
#include "library.h"
#include "log.h"

#include "qtfb/FBController.h"
#include "qtfb/fbmanagement.h"

#include <dlfcn.h>

void loadTestingModules(){
    const char *directoryPath = "testing_extensions";
    DIR *dir;
    struct dirent *entry;
    struct stat entryStat;

    dir = opendir(directoryPath);
    if (dir == nullptr) {
        CERR << "Unable to open directory" << std::endl;
        return;
    }

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char entryPath[300];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", directoryPath, entry->d_name);
        stat(entryPath, &entryStat);
        if (S_ISREG(entryStat.st_mode)) {
            void *dl = dlopen((const char *) entryPath, RTLD_NOW);
            if(!dl) continue;
            void (*extLoad)() = (void (*)()) dlsym(dl, "_ext_load");
            if(extLoad) extLoad();
            CERR << "Loaded test extension " << entryPath << std::endl;
        }
    }

    // Close the directory
    closedir(dir);

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
    const int emuOnlyTypeId = qmlRegisterSingletonType<AppLoadEmuOnly>("net.asivery.AppLoad", 1, 0, "AppLoadEmuOnly", &AppLoadEmuOnly::qmlSingleton);

    QDEBUG << "Registered QML modules:"
           << "net.asivery.AppLoad"
           << appLoadTypeId
           << coordinatorTypeId
           << libraryTypeId
           << applicationTypeId
           << launcherTypeId
           << emuOnlyTypeId
           << "net.asivery.Framebuffer"
           << fbControllerTypeId;
}

int main(int argc, char *argv[])
{
    loadTestingModules();

    QGuiApplication a(argc, argv);
    QQmlApplicationEngine engine;
    appload::library::loadApplications();
    qtfb::management::start();

    registerAppLoadQmlTypes();
    engine.load(QUrl(QStringLiteral("./_start.qml")));

    return a.exec();
}
