/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QtCore/QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QString>
#include "mediainfoplugin.h"

#define SETTINGS "/usr/share/meego-ux-media/lastfm.ini"

MediaInfoPlugin *instance = NULL;

MediaInfoPlugin::MediaInfoPlugin(QObject *parent) :
    MediaInfoPluginInterface(parent)
{
    QString iniFile(SETTINGS);
    if(QFile::exists(iniFile))
    {
        QSettings settings(iniFile, QSettings::NativeFormat);
        api_key = settings.value("api_key", "").toString();
        baseurl = settings.value("baseurl", "").toString();

        if(api_key.isEmpty())
            qDebug() << PLUGINNAME << "error, api_key key not found: " << iniFile;

        if(baseurl.isEmpty())
            qDebug() << PLUGINNAME << "error, baseurl key not found: " << iniFile;
    }
    else
    {
        qDebug() << PLUGINNAME << "error, settings file not found: " << iniFile;
    }

    QUrl httpProxy(getenv("http_proxy"));
    if (!httpProxy.isEmpty())
    {
        QNetworkProxy proxy(QNetworkProxy::HttpCachingProxy,
                            httpProxy.host(),
                            httpProxy.port());
        manager.setProxy(proxy);
    }
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(networkReply(QNetworkReply*)));
}

bool MediaInfoPlugin::supported(QString type)
{
    if(baseurl.isEmpty()||api_key.isEmpty())
        return false;

    if(type == "artistimage")
        return true;
    else if(type == "albumimage")
        return true;
    else
        return false;
}

bool MediaInfoPlugin::request(QString id, QString type, QString info)
{
    QStringList args = info.split("|", QString::KeepEmptyParts);
    QString url;
    CallerInfo cinfo;
    cinfo.id = id;
    cinfo.type = type;
    cinfo.info = info;
    qDebug() << "request: " << type << " " << info;

    if(baseurl.isEmpty()||api_key.isEmpty())
    {
        qDebug() << PLUGINNAME << "error, plugin is unconfigured";
        return false;
    }

    if(type == "artistimage")
    {
        if(args.count() != 2)
        {
            qDebug() << PLUGINNAME << "error, artistimage command badly formed";
            return false;
        }
        cinfo.artist = args[0];
        cinfo.thumburi = args[1];
        url = baseurl + "?method=artist.getinfo&artist=" +
                      cinfo.artist + "&api_key=" + api_key;
    }
    else if(type == "albumimage")
    {
        if(args.count() != 3)
        {
            qDebug() << PLUGINNAME << "error, albumimage command badly formed";
            return false;
        }
        cinfo.artist = args[0];
        cinfo.album = args[1];
        cinfo.thumburi = args[2];
        url = baseurl + "?method=album.getinfo&album=" +
                      cinfo.album + "&artist=" + cinfo.artist + "&api_key=" + api_key;
    }
    else
    {
        return false;
    }

    m_callers.insert(url, cinfo);
    qDebug() << id << " lastfm call: " << url;
    manager.get(QNetworkRequest(url));
    return true;
}

bool MediaInfoPlugin::networkRedirect(QNetworkReply *reply, CallerInfo &cinfo)
{
    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid())
    {
        qDebug() << "networkRedirect";
        m_callers.insert(redirect.toString(), cinfo);
        reply->deleteLater();
        manager.get(QNetworkRequest(redirect));
        return true;
    }
    return false;
}

void MediaInfoPlugin::networkReply(QNetworkReply *reply)
{
    /* retrieve this caller's info */
    QString url = reply->request().url().toString();

    if(!m_callers.contains(url))
        return;

    CallerInfo cinfo = m_callers[url];
    m_callers.remove(url);
    cinfo.data = reply->readAll();

    if(url.contains(".getinfo"))
    {
        if(networkRedirect(reply, cinfo))
            return;

        //qDebug() << cinfo.id << " lastfm return: " << url;
        if (!reply->error())
        {
            QHash<QString, QString> images;

            if(cinfo.type == "albumimage")
            {
                if (QFile::exists(cinfo.thumburi))
                {
                    emit ready(cinfo.id, "");
                    return;
                }
                if(!parseAlbumXml(images, cinfo))
                    return;
                if ((images.isEmpty())||
                    ((!images.contains("mega"))&&
                     (!images.contains("extralarge"))&&
                     (!images.contains("large"))&&
                     (!images.contains("medium"))&&
                     (!images.contains("small"))))
                {
                    emit error(cinfo.id, "No thumbnail url found in XML data");
                    return;
                }
            }
            else if(cinfo.type == "artistimage")
            {
                if (QFile::exists(cinfo.thumburi))
                {
                    emit ready(cinfo.id, "");
                    return;
                }
                if(!parseArtistXml(images, cinfo))
                    return;
                if ((images.isEmpty())||
                    ((!images.contains("mega"))&&
                     (!images.contains("extralarge"))&&
                     (!images.contains("large"))&&
                     (!images.contains("medium"))&&
                     (!images.contains("small"))))
                {
                    emit error(cinfo.id, "No thumbnail url found in XML data");
                    return;
                }
            }

            // Use the largest available image
            if (images.contains("mega"))
                url = images["mega"];
            else if (images.contains("extralarge"))
                url = images["extralarge"];
            else if (images.contains("large"))
                url = images["large"];
            else if (images.contains("medium"))
                url = images["medium"];
            else
                url = images["small"];

            m_callers.insert(url, cinfo);

            //qDebug() << cinfo.id << " image download call: " << url;
            manager.get(QNetworkRequest(QUrl(url)));
        }
        else
        {
            if (reply->error() == QNetworkReply::UnknownContentError)
                emit error(cinfo.id, "No Album Found");
            else
                emit error(cinfo.id, reply->errorString());
        }
    }
    else
    {
        //qDebug() << cinfo.id << " image download return: " << url;
        if (reply->error())
        {
            emit error(cinfo.id, reply->errorString());
            return;
        }

        QImage qimage;
        if (!qimage.loadFromData(cinfo.data))
            emit error(cinfo.id, "Unable to load image");

        if (qimage.save(cinfo.thumburi, "JPEG"))
            emit ready(cinfo.id, "");
        else
            emit error(cinfo.id, "Unable to convert image");
    }
}

bool MediaInfoPlugin::parseAlbumXml(QHash<QString, QString> &images, CallerInfo &cinfo)
{
    QXmlStreamReader m_xml(cinfo.data);
    while (!m_xml.atEnd())
    {
        m_xml.readNext();
        if (m_xml.isStartElement())
        {
            if (m_xml.name() == "image")
            {
                readImage(images, m_xml, m_xml.attributes().value("size").toString());
            }
        }
    }

    if (m_xml.error())
    {
        emit error(cinfo.id, m_xml.errorString());
        return false;
    }
    return true;
}

bool MediaInfoPlugin::parseArtistXml(QHash<QString, QString> &images, CallerInfo &cinfo)
{
    int depth = 0;

    QXmlStreamReader m_xml(cinfo.data);
    while (!m_xml.atEnd())
    {
        m_xml.readNext();
        if (m_xml.isStartElement())
        {
            // <lastfm><artist><image/></artist></lastfm>
            if (depth == 2 && m_xml.name() == "image")
            {
                readImage(images, m_xml, m_xml.attributes().value("size").toString());
            }
            else
            {
                depth++;
            }
        }
        else if (m_xml.isEndElement())
        {
            depth--;
        }
    }

    if (m_xml.error())
    {
        emit error(cinfo.id, m_xml.errorString());
        return false;
    }
    return true;
}

void MediaInfoPlugin::readImage(QHash<QString, QString> &images, QXmlStreamReader &m_xml, QString type)
{
    while (!m_xml.atEnd())
    {
        m_xml.readNext();
        if (m_xml.isEndElement())
        {
            break;
        }
        else if (m_xml.isCharacters() && !m_xml.isWhitespace())
        {
            images[type] = m_xml.text().toString();
        }
    }
}

QString MediaInfoPlugin::stripInvalidEntities(const QString &src)
{
    QString source(src);

    // http://live.gnome.org/MediaArtStorageSpec
    // remove blocks and special characters
    QRegExp removeblocks( "(\\([^\\)]*\\))"
                          "|(\\[[^\\]]*\\])"
                          "|(\\{[^\\}]*\\})"
                          "|(\\<[^\\>]*\\>)");
    source.replace(removeblocks, "");

    // remove special characters
    source.remove(QRegExp("[\\(\\)\\_\\{\\}\\[\\]\\!\\@\\#\\$\\^\\&\\*\\+\\=\\|\\\\\\/\\\"\\'\\?\\<\\>\\~\\`]+"));

    // replace tabs with spaces
    source.replace('\t', " ");

    // replace space sequences with single spaces
    source.replace(QRegExp("\\s+"), " ");

    // remove leading and trailing spaces
    source.remove(QRegExp("(^\\s+)|(\\s+$)"));

    if(source.isEmpty())
        return "";

    return source.toLower();
}

QString MediaInfoPlugin::getThumburi(const QString &artist, const QString &album)
{
    if(artist.isEmpty()||album.isEmpty())
        return "";

    QString albumInfo = stripInvalidEntities(album);
    QString artistInfo = stripInvalidEntities(artist);

    if(albumInfo.isEmpty()||artistInfo.isEmpty())
        return "";

    QByteArray album_md5 =
        QCryptographicHash::hash(albumInfo.toAscii(),
                                 QCryptographicHash::Md5);
    QByteArray artist_md5 =
        QCryptographicHash::hash(artistInfo.toAscii(),
                                 QCryptographicHash::Md5);

    return QDir::toNativeSeparators(QDir::homePath()) +
        QDir::separator() + QString(".cache/media-art/album-") +
        artist_md5.toHex() + "-" + album_md5.toHex() + ".jpeg";
}

QString MediaInfoPlugin::getThumburi(const QString &artist)
{
    if(artist.isEmpty())
        return "";

    QString artistInfo = stripInvalidEntities(artist);

    if(artistInfo.isEmpty())
        return "";

    QByteArray artist_md5 =
        QCryptographicHash::hash(artistInfo.toAscii(),
                                 QCryptographicHash::Md5);

    return QDir::toNativeSeparators(QDir::homePath()) +
        QDir::separator() + QString(".cache/media-art/artist-") +
        artist_md5.toHex() + ".jpeg";
}

Q_EXPORT_PLUGIN2(mediainfoplugin, MediaInfoPlugin);
