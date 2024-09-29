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
#ifndef LIB7Z_FACADE_H
#define LIB7Z_FACADE_H

#include "installer_global.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QPoint>
#include <QRunnable>
#include <QString>
#include <QVariant>
#include <QVector>

#include "Common/MyWindows.h"

#include <stdexcept>
#include <string>

QT_BEGIN_NAMESPACE
class QStringList;
template <typename T> class QVector;
QT_END_NAMESPACE

namespace Lib7z {
    class INSTALLER_EXPORT SevenZipException : public std::runtime_error {
    public:
        explicit SevenZipException( const QString& msg ) : std::runtime_error( msg.toStdString() ), m_message( msg ) {}
        explicit SevenZipException( const char* msg ) : std::runtime_error( msg ), m_message( QString::fromUtf8( msg ) ) {}
        explicit SevenZipException( const std::string& msg ) : std::runtime_error( msg ), m_message( QString::fromStdString( msg ) ) {}

        ~SevenZipException() throw() {}
        QString message() const { return m_message; }
    private:
        QString m_message;
    };

    class INSTALLER_EXPORT File {
    public:
        File();
        QVector<File> subtreeInPreorder() const;

        bool operator<( const File& other ) const;
        bool operator==( const File& other ) const;

        QFile::Permissions permissions;
        QString path;
        QDateTime mtime;
        quint64 uncompressedSize;
        quint64 compressedSize;
        bool isDirectory;
        QVector<File> children;
        QPoint archiveIndex;
    };

    class ExtractCallbackPrivate;
    class ExtractCallbackImpl;

    class ExtractCallback {
        friend class ::Lib7z::ExtractCallbackImpl;
    public:
        ExtractCallback();
        virtual ~ExtractCallback();

        void setTarget( QIODevice* archive );
        void setTarget( const QString& dir );

    protected:
        /**
         * Reimplement to prepare for file @p filename to be extracted, e.g. by renaming existing files.
         * @return @p true if the preparation was successful and extraction can be continued.
         * If @p false is returned, the extraction will be aborted. Default implementation returns @p true.
         */
        virtual bool prepareForFile( const QString& filename );
        virtual void setCurrentFile( const QString& filename );
        virtual HRESULT setCompleted( quint64 completed, quint64 total );

    public: //for internal use
        const ExtractCallbackImpl* impl() const;
        ExtractCallbackImpl* impl();

    private:
        ExtractCallbackPrivate* const d;
    };

    class UpdateCallbackPrivate;
    class UpdateCallbackImpl;

    class UpdateCallback
    {
        friend class ::Lib7z::UpdateCallbackImpl;
    public:
        UpdateCallback();
        virtual ~UpdateCallback();

        void setTarget( QIODevice* archive );
        void setSourcePaths( const QStringList& paths );

        virtual UpdateCallbackImpl* impl();

    private:
        UpdateCallbackPrivate* const d;
    };

    class OpenArchiveInfoCleaner : public QObject {
            Q_OBJECT
        public:
            OpenArchiveInfoCleaner() {}
        private Q_SLOTS:
            void deviceDestroyed(QObject*);
    };

    /*!
        Extracts the given File \a file from \a archive into output device \a out using the provided extract
        callback \a callback.

        Throws Lib7z::SevenZipException on error.
    */
    void INSTALLER_EXPORT extractFileFromArchive(QIODevice* archive, const File& item, QIODevice* out,
        ExtractCallback* callback=0 );

    /*!
        Extracts the given File \a file from \a archive into target directory \a targetDirectory using the
        provided extract callback \a callback. The output filename is deduced from the \a file path name.

        Throws Lib7z::SevenZipException on error.
    */
    void INSTALLER_EXPORT extractFileFromArchive(QIODevice* archive, const File& item,
        const QString& targetDirectory, ExtractCallback* callback = 0);

    /*!
        Extracts the given \a archive content into target directory \a targetDirectory using the
        provided extract callback \a callback. The output filenames are deduced from the \a archive content.

        Throws Lib7z::SevenZipException on error.
    */
    void INSTALLER_EXPORT extractArchive(QIODevice* archive, const QString& targetDirectory,
        ExtractCallback* callback = 0);

    /*
     * @thows Lib7z::SevenZipException
     */
    void INSTALLER_EXPORT createArchive( QIODevice* archive, const QStringList& sourcePaths, UpdateCallback* callback = 0 );

    /*
     * @throws Lib7z::SevenZipException
     */
    QVector<File> INSTALLER_EXPORT listArchive( QIODevice* archive );

    /*
     * @throws Lib7z::SevenZipException
     */
    bool INSTALLER_EXPORT isSupportedArchive( QIODevice* archive );

    /*
     * @throws Lib7z::SevenZipException
     */
    bool INSTALLER_EXPORT isSupportedArchive( const QString& archive );



    enum Error {
        NoError=0,
        Failed=1,
        UserDefinedError=128
    };

    class ExtractCallbackJobImpl;

    class INSTALLER_EXPORT Job : public QObject, public QRunnable {
        friend class ::Lib7z::ExtractCallbackJobImpl;
        Q_OBJECT
    public:

        explicit Job( QObject* parent=0 );
        ~Job();
        void start();
        int error() const;
        bool hasError() const;
        QString errorString() const;

        /* reimp */ void run();

    protected:
        void emitResult();
        void setError( int code );
        void setErrorString( const QString& err );
        void emitProgress( qint64 completed, qint64 total );

    Q_SIGNALS:
        void finished( Lib7z::Job* job );
        void progress( qint64 completed, qint64 total );

    private Q_SLOTS:
        virtual void doStart() = 0;

    private:
        class Private;
        Private* const d;
    };

    class INSTALLER_EXPORT ListArchiveJob : public Job {
        Q_OBJECT
    public:

        explicit ListArchiveJob( QObject* parent=0 );
        ~ListArchiveJob();

        QIODevice* archive() const;
        void setArchive( QIODevice* archive );

        QVector<File> index() const;

    private:
        /* reimp */ void doStart();

    private:
        class Private;
        Private* const d;
    };

    class INSTALLER_EXPORT ExtractItemJob : public Job {
        Q_OBJECT
        friend class ::Lib7z::ExtractCallback;
    public:

        explicit ExtractItemJob( QObject* parent=0 );
        ~ExtractItemJob();

        File item() const;
        void setItem( const File& item );

        QIODevice* archive() const;
        void setArchive( QIODevice* archive );

        QString targetDirectory() const;
        void setTargetDirectory( const QString& dir );

        void setTarget( QIODevice* dev );

    private:
        /* reimp */ void doStart();

    private:
        class Private;
        Private* const d;
    };

    QByteArray INSTALLER_EXPORT formatKeyValuePairs( const QVariantList& l );
}

#endif // LIB7Z_FACADE_H
