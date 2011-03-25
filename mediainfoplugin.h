/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef MEDIAINFOPLUGIN_H
#define MEDIAINFOPLUGIN_H

#include <QtCore>
#include <QtCore/QtGlobal>
#include <QHash>
#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QObject>
#include <mediainfoplugininterface.h>

#define PLUGINNAME "lastfm"

struct CallerInfo
{
    QString id;
    QString type;
    QString info;
    QString artist;
    QString album;
    QString thumburi;
    QByteArray data;
};

class MediaInfoPlugin : public MediaInfoPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(MediaInfoPluginInterface)

public:
    explicit MediaInfoPlugin(QObject *parent = 0);
    ~MediaInfoPlugin() {}
    bool request(QString id, QString type, QString info);
    bool supported(QString type);
    QString pluginName() { return PLUGINNAME; }

private slots:
    void networkReply(QNetworkReply *);

private:
    bool networkRedirect(QNetworkReply *reply, CallerInfo &cinfo);
    bool parseAlbumXml(QHash<QString, QString> &images, CallerInfo &cinfo);
    bool parseArtistXml(QHash<QString, QString> &images, CallerInfo &cinfo);
    void readImage(QHash<QString, QString> &images, QXmlStreamReader &m_xml, QString type);
    QString stripInvalidEntities(const QString &src);
    QString getThumburi(const QString &artist, const QString &album);
    QString getThumburi(const QString &artist);

    QNetworkAccessManager manager;
    QHash<QString, CallerInfo> m_callers;
    QSettings *settings;
    QString api_key;
    QString baseurl;
};

#endif // MEDIAINFOPLUGIN_H
