#include "cameramanager.h"
#include "dbusreceiver.h"

#include <QApplication>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuick>

#include <thread>

#include <glog/logging.h>

int main(int argc, char *argv[])
{
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  if (!QDBusConnection::sessionBus().isConnected()) {
    qCritical() << "Cannot connect to the D-Bus session bus.\n";
    return EXIT_FAILURE;
  }

  DBusReceiver &dbus = DBusReceiver::getInstance();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
  QApplication app(argc, argv);

  app.setOrganizationName("laserposition-estimator");
  app.setOrganizationDomain("laserposition-estimator");

  CameraManager manager;
  manager.makeWindow();
  std::thread manager_thread(&CameraManager::runCamera, std::ref(manager));
  std::thread waitkey_thread(&CameraManager::runWaitKey, std::ref(manager));

  dbus.setManager(manager);

  QQmlApplicationEngine engine;
  const QUrl url(QStringLiteral("qrc:/main.qml"));
  engine.rootContext()->setContextProperty("cameraManager", &manager);
  QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreated,
      &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);
  engine.load(url);

  int const result = app.exec();
  manager.stop();
  manager_thread.join();
  waitkey_thread.join();
  return result;
}
