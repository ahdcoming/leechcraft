/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2011  Minh Ngo
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "otzerkaludownloader.h"
#include <QWebElement>
#include <QWebPage>
#include <QRegExp>
#include <QFileInfo>
#include <QDir>
#include <QUrlQuery>
#include <util/xpc/util.h>

namespace LeechCraft
{
namespace Otzerkalu
{
	DownloadParams::DownloadParams (const QUrl& downloadUrl,
			const QString& destDir, int recLevel, bool fromOtherSite)
	: DownloadUrl_ (downloadUrl)
	, DestDir_ (destDir)
	, RecLevel_ (recLevel)
	, FromOtherSite_ (fromOtherSite)
	{
	}

	FileData::FileData (const QUrl& url,
			const QString& filename, int recLevel)
	: Url_ (url)
	, Filename_ (filename)
	, RecLevel_ (recLevel)
	{
	}

	OtzerkaluDownloader::OtzerkaluDownloader (const DownloadParams& param,
			int id, QObject *parent)
	: QObject (parent)
	, Param_ (param)
	, ID_ (id)
	{
	}

	QString OtzerkaluDownloader::GetLastDownloaded () const
	{
		return DownloadedFiles_.isEmpty () ? QString () : DownloadedFiles_.last ();
	}

	void OtzerkaluDownloader::Begin ()
	{
		//Let's download the first URL
		Download (Param_.DownloadUrl_, Param_.RecLevel_);
	}

	void OtzerkaluDownloader::HandleProvider (QObject *provider, int id,
			const QUrl& url, const QString& filename, int recLevel)
	{
		qDebug () << Q_FUNC_INFO
				<< "Downloading "
				<< url.toString ()
				<< "ID"
				<< id;
		FileMap_.insert (id, FileData (url, filename, recLevel));
		connect (provider,
				SIGNAL (jobFinished (int)),
				this,
				SLOT (handleJobFinished (int)),
				Qt::UniqueConnection);
	}

	int OtzerkaluDownloader::FilesCount () const
	{
		return DownloadedFiles_.count ();
	}

	QList<QUrl> OtzerkaluDownloader::CSSParser (const QString& data) const
	{
		QRegExp UrlCSS ("(?s).*?:\\s*url\\s*\\((.*?)\\).*");
		QList<QUrl> urlStack;
		int pos = 0;
		while ((pos = UrlCSS.indexIn (data, pos)) != -1)
		{
			const QUrl& url = UrlCSS.cap (1);
			if (!url.isValid ())
				continue;

			urlStack.append (url);
		}
		return urlStack;
	}

	QString OtzerkaluDownloader::CSSUrlReplace (QString value, const FileData& data)
	{
		for (const auto& urlCSS : CSSParser (value))
		{
			const auto& filename = Download (urlCSS, data.RecLevel_ - 1);
			if (!filename.isEmpty ())
				value.replace (urlCSS.toString (), filename);
		}
		return value;
	}

	void OtzerkaluDownloader::handleJobFinished (int id)
	{
		qDebug () << Q_FUNC_INFO << "Download finished";
		--UrlCount_;
		const FileData& data = FileMap_ [id];
		if (!data.RecLevel_ && Param_.RecLevel_)
			return;

		const QString& filename = data.Filename_;
		DownloadedFiles_.append (filename);
		emit fileDownloaded (ID_, DownloadedFiles_.count ());

		QFile file (filename);
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "Can't parse the file "
					<< filename
					<< ":"
					<< file.errorString ();
			return;
		}

		if (filename.section ('.', -1) == "css")
		{
			WriteData (filename, CSSUrlReplace (file.readAll (), data));
			return;
		}

		QWebPage page;
		page.mainFrame ()->setContent (file.readAll ());
		QWebElementCollection uel = page.mainFrame ()->findAllElements ("*[href]") +
				page.mainFrame ()->findAllElements ("*[src]");

		bool haveLink = false;
		for (QWebElementCollection::iterator urlElement = uel.begin ();
				urlElement != uel.end (); ++urlElement)
			if (HTMLReplace (urlElement, data))
				haveLink = true;

		QWebElementCollection styleColl = page.mainFrame ()->findAllElements ("style");
		for (QWebElementCollection::iterator styleItr = styleColl.begin ();
				styleItr != styleColl.end (); ++styleItr)
			(*styleItr).setInnerXml (CSSUrlReplace ((*styleItr).toInnerXml (), data));

		if (haveLink)
			WriteData (filename, page.mainFrame ()->toHtml ());

		if (!UrlCount_)
		{
			emit gotEntity (Util::MakeNotification ("Otzerkalu",
					tr ("Finished mirroring <em>%1</em>.")
						.arg (Param_.DownloadUrl_.toString ()),
					PInfo_));
			emit mirroringFinished (ID_);
		}
	}

	bool OtzerkaluDownloader::HTMLReplace (QWebElementCollection::iterator element,
			const FileData& data)
	{
		bool haveHref = true;
		QUrl url = (*element).attribute ("href");
		if (!url.isValid ())
		{
			url = (*element).attribute ("src");
			haveHref = false;
		}
		if (url.isRelative ())
			url = data.Url_.resolved (url);

		if (!Param_.FromOtherSite_ && url.host () != Param_.DownloadUrl_.host ())
			return false;

		const QString& filename = Download (url, data.RecLevel_ - 1);
		if (filename.isEmpty ())
			return false;

		(*element).setAttribute (haveHref ? "href" : "src", filename);
		return true;
	}

	QString OtzerkaluDownloader::Download (const QUrl& url, int recLevel)
	{
		const QFileInfo fi (url.path ());
		const QString& name = fi.fileName ();
		const QString& path = Param_.DestDir_ + '/' + url.host () +
				fi.path ();

		//If file name's empty, rename it to 'index.html'
		const QString& file = path + '/' + (name.isEmpty () ? "index.html" : name);

		//If file's not a html file, add .html tail to the name
		const QString& filename = url.hasQuery () ?
				file + "?" + QUrlQuery { url }.toString (QUrl::FullyDecoded) + ".html" :
				file;

		//If a file's downloaded
		if (DownloadedFiles_.contains (filename))
			return QString ();

		//Create the necessary directory for the downloaded file
		QDir::root ().mkpath (path);

		int id = -1;
		QObject *pr;
		Entity e = Util::MakeEntity (url,
				filename,
				LeechCraft::Internal |
					LeechCraft::DoNotNotifyUser |
					LeechCraft::DoNotSaveInHistory |
					LeechCraft::NotPersistent |
					LeechCraft::DoNotAnnounceEntity);
		emit delegateEntity (e, &id, &pr);
		if (id == -1)
		{
			qWarning () << Q_FUNC_INFO
					<< "could not download"
					<< url
					<< "to"
					<< filename;
			emit gotEntity (Util::MakeNotification ("Otzerkalu",
					tr ("Could not download %1")
						.arg (url.toString ()),
					PCritical_));
			return QString ();
		}
		++UrlCount_;
		HandleProvider (pr, id, url, filename, recLevel);

		return filename;
	}

	bool OtzerkaluDownloader::WriteData(const QString& filename, const QString& data)
	{
		QFile file (filename);
		if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
			return false;

		QTextStream out (&file);
		out << data;
		return true;
	}
}
}
