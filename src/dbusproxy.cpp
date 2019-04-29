#include "dbusproxy.h"

DBusProxy::DBusProxy(SystemTray &t, Widget &w, QObject *parent):tray(t),widget(w)
{

}

void DBusProxy::IndexChangeFiles(QStringList paths)
{
    //TODO start Index
    this->widget.IndexSomeFiles(paths);

}

void DBusProxy::showWindow()
{
    //TODO
    this->tray.showWindow();

}