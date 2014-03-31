#include <kaction.h>
#include <kactionmenu.h>
#include <kdemacros.h>
#include <kfileitem.h>
#include <klocalizedstring.h>
#include <kfileitemlistproperties.h>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QApplication>
#include <QClipboard>
#include <QtDBus>
#include <QtDebug>
#include <KDE/KPluginFactory>
#include <KDE/KPluginLoader>
#include "dolphin-meocloud-plugin.h"

K_PLUGIN_FACTORY(DolphinMEOCloudPluginFactory, registerPlugin<DolphinMEOCloudPlugin>();)
K_EXPORT_PLUGIN(DolphinMEOCloudPluginFactory("dolphin-meocloud-plugin"))

DolphinMEOCloudPlugin::DolphinMEOCloudPlugin(QObject* parent, const QVariantList & args):
    KVersionControlPlugin(parent),
    m_versionInfoHash()
{
    Q_UNUSED(args);

    QString lang = QLocale::system().uiLanguages().first().replace("-", "_");

    QString SHARE_STRING = "Share Folder";
    QString BROWSER_STRING = "Open in Browser";
    QString LINK_STRING = "Copy Link";

    if(lang == "pt" || lang == "pt_PT" || lang == "pt_BR")
    {
        SHARE_STRING = "Partilhar Pasta";
        BROWSER_STRING = "Abrir no navegador web";
        LINK_STRING = "Copiar hiperligação";
    }

    m_shareFolderAction = new KAction(this);
    m_shareFolderAction->setIcon(KIcon("internet-web-browser"));
    m_shareFolderAction->setText(SHARE_STRING);
    connect(m_shareFolderAction, SIGNAL(triggered()),
            this, SLOT(shareFolderAction()));

    m_openInBrowserAction = new KAction(this);
    m_openInBrowserAction->setIcon(KIcon("internet-web-browser"));
    m_openInBrowserAction->setText(BROWSER_STRING);
    connect(m_openInBrowserAction, SIGNAL(triggered()),
            this, SLOT(openInBrowserAction()));

    m_shareFileLinkAction = new KAction(this);
    m_shareFileLinkAction->setIcon(KIcon("internet-web-browser"));
    m_shareFileLinkAction->setText(LINK_STRING);
    connect(m_shareFileLinkAction, SIGNAL(triggered()),
            this, SLOT(shareFileLinkAction()));

    ShellServer *server = new ShellServer(this);
    new ShellAdaptor(server);

    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.unregisterService("pt.meocloud.shell");
    bool ret = connection.registerService("pt.meocloud.shell");
    if (!ret)
        qDebug() << "registerService failed :(";
    connection.unregisterObject("/pt/meocloud/shell");
    ret = connection.registerObject("/pt/meocloud/shell", server);
    if (!ret)
        qDebug() << "registerObject failed :(";
}

DolphinMEOCloudPlugin::~DolphinMEOCloudPlugin()
{
}

QString DolphinMEOCloudPlugin::fileName() const
{
    return QLatin1String(".meocloud");
}

bool DolphinMEOCloudPlugin::beginRetrieval(const QString& directory)
{
    Q_ASSERT(directory.endsWith(QLatin1Char('/')));

    QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                    "/pt/meocloud/dbus",
                                                    "",
                                                    "GetCloudHome");
    QDBusMessage response = QDBusConnection::sessionBus().call(m);

    QString cloud_home;

    if (response.type() == QDBusMessage::ReplyMessage)
    {
        cloud_home = response.arguments().at(0).value<QString>();
    }
    else
    {
        return true;
    }

    bool cloud_is_here = false;
    QDir dir(directory);
    QStringList files = dir.entryList();

    for(int i=2;i<files.size();++i)
    {
        QString filename = dir.absolutePath() + QDir::separator() + files.at(i);

        if(filename == cloud_home)
        {
            cloud_is_here = true;
            break;
        }
    }

    if(!directory.startsWith(cloud_home) && !cloud_is_here)
        return true;

    m_versionInfoHash.clear();

    for(int i=2;i<files.size();++i)
    {
        QString filename = dir.absolutePath() + QDir::separator() + files.at(i);
        KVersionControlPlugin::VersionState versionstate;

        if(filename == cloud_home)
        {
            QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                            "/pt/meocloud/dbus",
                                                            "",
                                                            "Status");
            QDBusMessage response = QDBusConnection::sessionBus().call(m);

            if (response.type() == QDBusMessage::ReplyMessage)
            {
                int status = response.arguments().at(0).value<int>();

                switch(status)
                {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                        versionstate = KVersionControlPlugin::UpdateRequiredVersion;
                        break;
                    default:
                        versionstate = KVersionControlPlugin::NormalVersion;
                        break;
                }

                m_versionInfoHash.insert(filename, versionstate);
            }

            return true;
        }
        else
        {
            versionstate = KVersionControlPlugin::UnversionedVersion;
        }

        QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                        "/pt/meocloud/dbus",
                                                        "",
                                                        "FileInCloud");
        m << filename;
        QDBusMessage response = QDBusConnection::sessionBus().call(m);

        if(response.type() == QDBusMessage::ReplyMessage)
        {
            bool in_cloud = response.arguments().at(0).value<bool>();
            bool is_syncing = response.arguments().at(1).value<bool>();

            if(in_cloud && is_syncing)
            {
                versionstate = KVersionControlPlugin::UpdateRequiredVersion;
                m_versionInfoHash.insert(filename, versionstate);
            }
            else if(in_cloud)
            {
                versionstate = KVersionControlPlugin::NormalVersion;
                m_versionInfoHash.insert(filename, versionstate);
            }
        }
    }

    return true;
}

void DolphinMEOCloudPlugin::endRetrieval()
{
}

KVersionControlPlugin::VersionState DolphinMEOCloudPlugin::versionState(const KFileItem& item)
{
    const QString itemUrl = item.localPath();

    if(m_versionInfoHash.contains(itemUrl))
    {
        return m_versionInfoHash.value(itemUrl);
    }

    return KVersionControlPlugin::UnversionedVersion;
}

void DolphinMEOCloudPlugin::setVersionState()
{
    emit versionStatesChanged();
}

QList<QAction*> DolphinMEOCloudPlugin::contextMenuActions(const KFileItemList& items)
{
    Q_ASSERT(!items.isEmpty());

    if(items.size() > 1 || items.size() == 0)
    {
        QList<QAction*> emptyactions;
        m_contextUrl = QString();
        return emptyactions;
    }

    KFileItem item = items.at(0);
    return getActions(item.url().path(), item.isDir());
}

QList<QAction*> DolphinMEOCloudPlugin::contextMenuActions(const QString& directory)
{
    return getActions(directory, true);
}

QList<QAction*> DolphinMEOCloudPlugin::getActions(QString path, bool is_dir)
{
    QList<QAction*> actions;

    QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                    "/pt/meocloud/dbus",
                                                    "",
                                                    "FileInCloud");
    m << path;
    QDBusMessage response = QDBusConnection::sessionBus().call(m);

    if(response.type() == QDBusMessage::ReplyMessage)
    {
        bool in_cloud = response.arguments().at(0).value<bool>();

        if (!in_cloud)
            return actions;
    }
    else
    {
        return actions;
    }

    m_contextUrl = path;

    KActionMenu* menuAction = new KActionMenu(this);
    menuAction->setText("MEO Cloud");

    menuAction->addAction(m_openInBrowserAction);

    if(is_dir)
        menuAction->addAction(m_shareFolderAction);
    else
        menuAction->addAction(m_shareFileLinkAction);

    actions.append(menuAction);
    return actions;
}

void DolphinMEOCloudPlugin::shareFolderAction()
{
    this->requestShare(m_contextUrl);
}

void DolphinMEOCloudPlugin::openInBrowserAction()
{
    this->requestOpen(m_contextUrl);
}

void DolphinMEOCloudPlugin::shareFileLinkAction()
{
    this->requestLink(m_contextUrl);
}

void DolphinMEOCloudPlugin::requestLink(QString path)
{
    QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                    "/pt/meocloud/dbus",
                                                    "",
                                                    "ShareLink");
    m << path;
    QDBusMessage response = QDBusConnection::sessionBus().call(m);
}

void DolphinMEOCloudPlugin::requestShare(QString path)
{
    QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                    "/pt/meocloud/dbus",
                                                    "",
                                                    "ShareFolder");
    m << path;
    QDBusMessage response = QDBusConnection::sessionBus().call(m);
}

void DolphinMEOCloudPlugin::requestOpen(QString path)
{
    QDBusMessage m = QDBusMessage::createMethodCall("pt.meocloud.dbus",
                                                    "/pt/meocloud/dbus",
                                                    "",
                                                    "OpenInBrowser");
    m << path;
    QDBusMessage response = QDBusConnection::sessionBus().call(m);
}
