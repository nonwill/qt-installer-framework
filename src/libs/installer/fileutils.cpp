/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "fileutils.h"

#include <errors.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QEventLoop>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QImageReader>

#include <errno.h>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace QInstaller;


// -- TempDirDeleter

TempDirDeleter::TempDirDeleter(const QString &path)
{
    m_paths.insert(path);
}

TempDirDeleter::TempDirDeleter(const QStringList &paths)
    : m_paths(paths.toSet())
{
}

TempDirDeleter::~TempDirDeleter()
{
    releaseAndDeleteAll();
}

QStringList TempDirDeleter::paths() const
{
    return m_paths.toList();
}

void TempDirDeleter::add(const QString &path)
{
    m_paths.insert(path);
}

void TempDirDeleter::add(const QStringList &paths)
{
    m_paths += paths.toSet();
}

void TempDirDeleter::releaseAll()
{
    m_paths.clear();
}

void TempDirDeleter::release(const QString &path)
{
    m_paths.remove(path);
}

void TempDirDeleter::passAndReleaseAll(TempDirDeleter &tdd)
{
    tdd.m_paths = m_paths;
    releaseAll();
}

void TempDirDeleter::passAndRelease(TempDirDeleter &tdd, const QString &path)
{
    tdd.add(path);
    release(path);
}

void TempDirDeleter::releaseAndDeleteAll()
{
    foreach (const QString &path, m_paths)
        releaseAndDelete(path);
}

void TempDirDeleter::releaseAndDelete(const QString &path)
{
    if (m_paths.contains(path)) {
        try {
            m_paths.remove(path);
            removeDirectory(path);
        } catch (const Error &e) {
            qCritical() << Q_FUNC_INFO << "Exception caught:" << e.message();
        } catch (...) {
            qCritical() << Q_FUNC_INFO << "Unknown exception caught.";
        }
    }
}

QString QInstaller::humanReadableSize(const qint64 &size, int precision)
{
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << QCoreApplication::translate("QInstaller", "bytes")
                 << QCoreApplication::translate("QInstaller", "KiB")
                 << QCoreApplication::translate("QInstaller", "MiB")
                 << QCoreApplication::translate("QInstaller", "GiB")
                 << QCoreApplication::translate("QInstaller", "TiB")
                 << QCoreApplication::translate("QInstaller", "PiB")
                 << QCoreApplication::translate("QInstaller", "EiB")
                 << QCoreApplication::translate("QInstaller", "ZiB")
                 << QCoreApplication::translate("QInstaller", "YiB");

    QStringListIterator it(measures);
    QString measure(it.next());

    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', precision).arg(measure);
}



// -- read, write operations

bool QInstaller::isLocalUrl(const QUrl &url)
{
    return url.scheme().isEmpty() || url.scheme().toLower() == QLatin1String("file");
}

QString QInstaller::pathFromUrl(const QUrl &url)
{
    if (isLocalUrl(url))
        return url.toLocalFile();
    const QString str = url.toString();
    if (url.scheme() == QLatin1String("resource"))
        return str.mid(QString::fromLatin1("resource").length());
    return str;
}

void QInstaller::openForRead(QIODevice *dev, const QString &name)
{
    Q_ASSERT(dev);
    if (!dev->open(QIODevice::ReadOnly))
        throw Error(QObject::tr("Cannot open file %1 for reading: %2").arg(name, dev->errorString()));
}

void QInstaller::openForWrite(QIODevice *dev, const QString &name)
{
    Q_ASSERT(dev);
    if (!dev->open(QIODevice::WriteOnly))
        throw Error(QObject::tr("Cannot open file %1 for writing: %2").arg(name, dev->errorString()));
}

void QInstaller::openForAppend(QIODevice *dev, const QString &name)
{
    Q_ASSERT(dev);
    if (!dev->open(QIODevice::ReadWrite | QIODevice::Append))
        throw Error(QObject::tr("Cannot open file %1 for writing: %2").arg(name, dev->errorString()));
}

qint64 QInstaller::blockingWrite(QIODevice *out, const char *buffer, qint64 size)
{
    qint64 left = size;
    while (left > 0) {
        const qint64 n = out->write(buffer, left);
        if (n < 0) {
            throw Error(QObject::tr("Write failed after %1 bytes: %2").arg(QString::number(size-left),
                out->errorString()));
        }
        left -= n;
    }
    return size;
}

qint64 QInstaller::blockingWrite(QIODevice *out, const QByteArray &ba)
{
    return blockingWrite(out, ba.constData(), ba.size());
}

qint64 QInstaller::blockingRead(QIODevice *in, char *buffer, qint64 size)
{
    if (in->atEnd())
        return 0;
    qint64 left = size;
    while (left > 0) {
        const qint64 n = in->read(buffer, left);
        if (n < 0) {
            throw Error(QObject::tr("Read failed after %1 bytes: %2").arg(QString::number(size-left),
                in->errorString()));
        }
        left -= n;
        buffer += n;
    }
    return size;
}

void QInstaller::blockingCopy(QIODevice *in, QIODevice *out, qint64 size)
{
    static const qint64 blockSize = 4096;
    QByteArray ba(blockSize, '\0');
    qint64 actual = qMin(blockSize, size);
    while (actual > 0) {
        blockingRead(in, ba.data(), actual);
        blockingWrite(out, ba.constData(), actual);
        size -= actual;
        actual = qMin(blockSize, size);
    }
}

void QInstaller::removeFiles(const QString &path, bool ignoreErrors)
{
    const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden);
    foreach (const QFileInfo &fi, entries) {
        if (fi.isSymLink() || fi.isFile()) {
            QFile f(fi.filePath());
            if (!f.remove()) {
                QString errorMessage = QObject::tr("Could not remove file %1: %2").arg(f.fileName(), f.errorString());
                if (ignoreErrors)
                    qWarning() << errorMessage;
                else
                    throw Error(errorMessage);
            }
        }
    }
}

static QString errnoToQString(int error)
{
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
    char msg[128];
    if (strerror_s(msg, sizeof msg, error) != 0)
        return QString::fromLocal8Bit(msg);
    return QString();
#else
    return QString::fromLocal8Bit(strerror(error));
#endif
}

void QInstaller::removeDirectory(const QString &path, bool ignoreErrors)
{
    if (path.isEmpty()) // QDir("") points to the working directory! We never want to remove that one.
        return;

    QStringList dirs;
    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        dirs.prepend(it.next());
        removeFiles(dirs.at(0), ignoreErrors);
    }

    QDir d;
    dirs.append(path);
    removeFiles(path, ignoreErrors);
    foreach (const QString &dir, dirs) {
        errno = 0;
        if (d.exists(path) && !d.rmdir(dir)) {
            QString errorMessage = QObject::tr("Could not remove folder %1: %2").arg(dir,
                errnoToQString(errno));
            if (ignoreErrors)
                qWarning() << errorMessage;
            else
                throw Error(errorMessage);
        }
    }
}

/*!
 \internal
 */
class RemoveDirectoryThread : public QThread
{
public:
    explicit RemoveDirectoryThread(const QString &path, bool ignoreErrors = false, QObject *parent = 0)
        : QThread(parent)
        , p(path)
        , ignore(ignoreErrors)
    {
    }

    const QString &error() const
    {
        return err;
    }

protected:
    /*!
     \reimp
     */
    void run()
    {
        try {
            removeDirectory(p, ignore);
        } catch (const Error &e) {
            err = e.message();
        }
    }

private:
    QString err;
    const QString p;
    const bool ignore;
};

void QInstaller::removeDirectoryThreaded(const QString &path, bool ignoreErrors)
{
    RemoveDirectoryThread thread(path, ignoreErrors);
    QEventLoop loop;
    QObject::connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
    thread.start();
    loop.exec();
    if (!thread.error().isEmpty())
        throw Error(thread.error());
}

void QInstaller::removeSystemGeneratedFiles(const QString &path)
{
    if (path.isEmpty())
        return;
#if defined Q_OS_MAC
    QFile::remove(path + QLatin1String("/.DS_Store"));
#elif defined Q_OS_WIN
    QFile::remove(path + QLatin1String("/Thumbs.db"));
#endif
}

void QInstaller::copyDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo(targetDir).exists() || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir))
        throw Error(QObject::tr("Could not create folder %1").arg(targetDir));

    QDirIterator it(sourceDir, QDir::NoDotAndDotDot | QDir::AllEntries);
    while (it.hasNext()) {
        const QFileInfo i(it.next());
        if (i.isDir()) {
            copyDirectoryContents(QDir(sourceDir).absoluteFilePath(i.fileName()),
                QDir(targetDir).absoluteFilePath(i.fileName()));
        } else {
            QFile f(i.filePath());
            const QString target = QDir(targetDir).absoluteFilePath(i.fileName());
            if (!f.copy(target)) {
                throw Error(QObject::tr("Could not copy file from %1 to %2: %3").arg(f.fileName(), target,
                    f.errorString()));
            }
        }
    }
}

void QInstaller::moveDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo(targetDir).exists() || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir))
        throw Error(QObject::tr("Could not create folder %1").arg(targetDir));

    QDirIterator it(sourceDir, QDir::NoDotAndDotDot | QDir::AllEntries);
    while (it.hasNext()) {
        const QFileInfo i(it.next());
        if (i.isDir()) {
            // only copy directories that are not the target to avoid loop dir creations
            QString newSource = QDir(sourceDir).absoluteFilePath(i.fileName());
            if (QDir(newSource) != QDir(targetDir)) {
                moveDirectoryContents(newSource, QDir(targetDir).absoluteFilePath(i.fileName()));
            }
        } else {
            QFile f(i.filePath());
            const QString target = QDir(targetDir).absoluteFilePath(i.fileName());
            if (!f.rename(target)) {
                throw Error(QObject::tr("Could not move file from %1 to %2: %3").arg(f.fileName(), target,
                    f.errorString()));
            }
        }
    }
}

void QInstaller::mkdir(const QString &path)
{
    errno = 0;
    if (!QDir().mkdir(QFileInfo(path).absoluteFilePath()))
        throw Error(QObject::tr("Could not create folder %1: %2").arg(path, errnoToQString(errno)));
}

void QInstaller::mkpath(const QString &path)
{
    errno = 0;
    if (!QDir().mkpath(QFileInfo(path).absoluteFilePath()))
        throw Error(QObject::tr("Could not create folder %1: %2").arg(path, errnoToQString(errno)));
}

QString QInstaller::generateTemporaryFileName(const QString &templ)
{
    if (templ.isEmpty()) {
        QTemporaryFile f;
        if (!f.open())
            throw Error(QObject::tr("Could not open temporary file: %1").arg(f.errorString()));
        return f.fileName();
    }

    static const QString characters = QLatin1String("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
    QString suffix;
    qsrand(qrand() * QDateTime::currentDateTime().toTime_t());
    for (int i = 0; i < 5; ++i)
        suffix += characters[qrand() % characters.length()];

    const QString tmp = QLatin1String("%1.tmp.%2.%3");
    int count = 1;
    while (QFile::exists(tmp.arg(templ, suffix).arg(count)))
        ++count;

    QFile f(tmp.arg(templ, suffix).arg(count));
    if (!f.open(QIODevice::WriteOnly))
        throw Error(QObject::tr("Could not open temporary file for template %1: %2").arg(templ, f.errorString()));
    f.remove();
    return f.fileName();
}

static char *installer_mkdtemp(char *templateName)
{
    QDir newDir;
#if defined(Q_OS_QNX ) || defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    const size_t length = strlen(templateName);

    char *XXXXXX = templateName + length - 6;

    if ((length < 6u) || strncmp(XXXXXX, "XXXXXX", 6))
        return 0;

    for (int i = 0; i < 256; ++i) {
        /* Fill in the random bits.  */
        int v = qrand();
        XXXXXX[0] = letters[v % 62];
        v /= 62;
        XXXXXX[1] = letters[v % 62];
        v = qrand();
        XXXXXX[2] = letters[v % 62];
        v /= 62;
        XXXXXX[3] = letters[v % 62];
        v = qrand();
        XXXXXX[4] = letters[v % 62];
        v /= 62;
        XXXXXX[5] = letters[v % 62];

        QString templateNameStr = QFile::decodeName(templateName);

        newDir = QDir(templateNameStr);
        if (newDir.mkpath(templateNameStr))
            return templateName;
    }
    return 0;
#else
    return mkdtemp(templateName);
#endif
}

QString QInstaller::createTemporaryDirectory(const QString &templateName)
{
    // TODO: use QTemporaryDir if we switched to Qt5
    QString path = QDir::tempPath() + QLatin1String("/") + templateName + QLatin1String("XXXXXX");
    path = QFile::decodeName(installer_mkdtemp(QFile::encodeName(path).data()));
    if (path.isEmpty()) {
        QTemporaryFile f(path);
        if (!f.open()) {
            throw Error(QObject::tr("Could not create temporary directory at %1: %2"
                ).arg(QFileInfo(f).absolutePath(), f.errorString()));
        } else {
            throw Error(QObject::tr("Could not create temporary directory at %1: unknown error"
                ).arg(QFileInfo(f).absolutePath()));
        }
    }

    qDebug() << "Temporary directory created:" << path;
    return path;
}

#ifdef Q_OS_WIN
#include <qt_windows.h>

QString QInstaller::getShortPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetShortPathName(nameC, NULL, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetShortPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

QString QInstaller::getLongPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetLongPathName(nameC, NULL, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetLongPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

QString QInstaller::normalizePathName(const QString &name)
{
    QString canonicalName = getShortPathName(name);
    if (canonicalName.isEmpty())
        return name;
    canonicalName = getLongPathName(canonicalName);
    if (canonicalName.isEmpty())
        return name;
    // Upper case drive letter
    if (canonicalName.size() > 2 && canonicalName.at(1) == QLatin1Char(':'))
        canonicalName[0] = canonicalName.at(0).toUpper();
    return canonicalName;
}

#pragma pack(push)
#pragma pack(2)

typedef struct {
    BYTE bWidth; // Width, in pixels, of the image
    BYTE bHeight; // Height, in pixels, of the image
    BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
    BYTE bReserved; // Reserved
    WORD wPlanes; // Color Planes
    WORD wBitCount; // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    DWORD dwImageOffset; // the ID
} ICONDIRENTRY;

typedef struct {
    WORD idReserved; // Reserved (must be 0)
    WORD idType; // Resource type (1 for icons)
    WORD idCount; // How many images?
    ICONDIRENTRY idEntries[1]; // The entries for each image
} ICONDIR;

typedef struct {
    BYTE bWidth; // Width, in pixels, of the image
    BYTE bHeight; // Height, in pixels, of the image
    BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
    BYTE bReserved; // Reserved
    WORD wPlanes; // Color Planes
    WORD wBitCount; // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    WORD nID; // the ID
} GRPICONDIRENTRY, *LPGRPICONDIRENTRY;

typedef struct {
    WORD idReserved; // Reserved (must be 0)
    WORD idType; // Resource type (1 for icons)
    WORD idCount; // How many images?
    GRPICONDIRENTRY idEntries[1]; // The entries for each image
} GRPICONDIR, *LPGRPICONDIR;


#pragma pack(pop)

void QInstaller::setApplicationIcon(const QString &application, const QString &icon)
{
    QFile iconFile(icon);
    if (!iconFile.open(QIODevice::ReadOnly)) {
        qWarning() << QString::fromLatin1("Could not use '%1' as application icon: %2.")
            .arg(icon, iconFile.errorString());
        return;
    }

    if (QImageReader::imageFormat(icon) != "ico") {
        qWarning() << QString::fromLatin1("Could not use '%1' as application icon, unsupported format %2.")
            .arg(icon, QLatin1String(QImageReader::imageFormat(icon)));
        return;
    }

    QByteArray temp = iconFile.readAll();
    ICONDIR* ig = reinterpret_cast<ICONDIR*> (temp.data());

    DWORD newSize = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (ig->idCount - 1);
    GRPICONDIR* newDir = reinterpret_cast< GRPICONDIR* >(new char[newSize]);
    newDir->idReserved = ig->idReserved;
    newDir->idType = ig->idType;
    newDir->idCount = ig->idCount;

    HANDLE updateRes = BeginUpdateResourceW((wchar_t*)QDir::toNativeSeparators(application).utf16(), false);
    for (int i = 0; i < ig->idCount; ++i) {
        char* temp1 = temp.data() + ig->idEntries[i].dwImageOffset;
        DWORD size1 = ig->idEntries[i].dwBytesInRes;

        newDir->idEntries[i].bWidth = ig->idEntries[i].bWidth;
        newDir->idEntries[i].bHeight = ig->idEntries[i].bHeight;
        newDir->idEntries[i].bColorCount = ig->idEntries[i].bColorCount;
        newDir->idEntries[i].bReserved = ig->idEntries[i].bReserved;
        newDir->idEntries[i].wPlanes = ig->idEntries[i].wPlanes;
        newDir->idEntries[i].wBitCount = ig->idEntries[i].wBitCount;
        newDir->idEntries[i].dwBytesInRes = ig->idEntries[i].dwBytesInRes;
        newDir->idEntries[i].nID = i + 1;

        UpdateResourceW(updateRes, RT_ICON, MAKEINTRESOURCE(i + 1),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), temp1, size1);
    }

    UpdateResourceW(updateRes, RT_GROUP_ICON, L"IDI_ICON1", MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        newDir, newSize);

    delete [] newDir;

    EndUpdateResourceW(updateRes, false);
}

static quint64 symlinkSizeWin(const QString &path)
{
    WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
    if (GetFileAttributesEx((wchar_t*)(path.utf16()), GetFileExInfoStandard, &fileAttributeData) == FALSE)
        return quint64(0);

    LARGE_INTEGER size;
    size.LowPart = fileAttributeData.nFileSizeLow;
    size.HighPart = fileAttributeData.nFileSizeHigh;
    return quint64(size.QuadPart);
}

#endif

quint64 QInstaller::fileSize(const QFileInfo &info)
{
    if (!info.isSymLink())
        return info.size();

#ifndef Q_OS_WIN
    struct stat buffer;
    if (lstat(qPrintable(info.absoluteFilePath()), &buffer) != 0)
        return quint64(0);
    return quint64(buffer.st_size);
#else
    return symlinkSizeWin(info.absoluteFilePath());
#endif
}
