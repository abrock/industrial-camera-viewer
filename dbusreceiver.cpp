#include "cameramanager.h"
#include "misc.h"

#include "dbusreceiver.h"

#include <tclap/CmdLine.h>

#include "captureinterface.hpp"

#include <ParallelTime/paralleltime.h>

DBusReceiver::DBusReceiver()
{
  std::cout << "Setting up DBusReceiver" << std::endl;
  QDBusConnection::sessionBus().registerObject("/", this);
  QDBusConnection::sessionBus().connect(QString(),
                                        QString(),
                                        CaptureTCLAPInterface::staticInterfaceName(),
                                        CaptureTCLAPInterface::staticRequestName(),
                                        this,
                                        SLOT(dbusRequestSlot(QString)));
  std::cout << "Finished setting up DBusReceiver" << std::endl;
}

DBusReceiver &DBusReceiver::getInstance()
{
  static DBusReceiver instance;
  std::cout << "Produced an instance of DBusReceiver" << std::endl;
  return instance;
}

void DBusReceiver::setManager(CameraManager &_manager)
{
  getInstance().manager = &_manager;
}

namespace {
void requestThread(QString const a)
{
  std::stringstream return_msg;
  return_msg << "Message received: " << std::endl << a.toStdString() << std::endl;

  std::cout << "Received message: " << std::endl << a.toStdString() << std::endl;

  DBusReceiver &dbus = DBusReceiver::getInstance();

  try {

    TCLAP::CmdLine cmd("Cmdline for DBus command parsing", ' ', "0.1");
    cmd.setExceptionHandling(false);

    std::vector<std::string> vec = Misc::splitString(a.toStdString(), ' ');
    cmd.parse(vec);

    // TODO: Do something meaningful
  }
  catch (TCLAP::ArgException const &e) {
    return_msg << " got an ArgException: " << std::endl << e.what();
  }
  catch (std::exception const &e) {
    return_msg << " got an exception: " << std::endl << e.what();
  }
  catch (...) {
    return_msg << " got an unknown exception: " << std::endl;
  }

  dbus.sendDbus(QString::fromStdString(return_msg.str()));
}
}  // namespace

void DBusReceiver::dbusRequestSlot(QString const a)
{
  std::thread t(requestThread, a);
  t.detach();
}

void DBusReceiver::sendDbus(const QString a)
{
  std::cout << "Sending response: " << a.toStdString() << std::endl;
  QDBusMessage msg = QDBusMessage::createSignal("/",
                                                CaptureTCLAPInterface::staticInterfaceName(),
                                                CaptureTCLAPInterface::staticResponseName());
  QString b;
  msg << a << b;
  QDBusConnection::sessionBus().send(msg);
}
