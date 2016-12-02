/**********************************************************************
 *  LeechCraft - modular cross-platform feature rich internet client.
 *  Copyright (C) 2010-2015  Oleg Linkin
 *
 *  Boost Software License - Version 1.0 - August 17th, 2003
 *
 *  Permission is hereby granted, free of charge, to any person or organization
 *  obtaining a copy of the software and accompanying documentation covered by
 *  this license (the "Software") to use, reproduce, display, distribute,
 *  execute, and transmit the Software, and to prepare derivative works of the
 *  Software, and to permit third-parties to whom the Software is furnished to
 *  do so, all subject to the following:
 *
 *  The copyright notices in the Software and this entire statement, including
 *  the above license grant, this restriction and the following disclaimer,
 *  must be included in all copies of the Software, in whole or in part, and
 *  all derivative works of the Software, unless such copies or derivative
 *  works are solely in the form of machine-executable object code generated by
 *  a source language processor.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 *  SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 *  FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 **********************************************************************/

#include "downmanager.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <interfaces/core/ientitymanager.h>
#include <interfaces/netstoremanager/istorageaccount.h>
#include <util/xpc/util.h>

namespace LeechCraft
{
namespace NetStoreManager
{
	DownManager::DownManager (ICoreProxy_ptr proxy, QObject *parent)
	: QObject { parent }
	, Proxy_ (proxy)
	{
	}

	void DownManager::SendEntity (const Entity& e)
	{
		Proxy_->GetEntityManager ()->HandleEntity (e);
	}

	void DownManager::DelegateEntity (const Entity& e, const QString& targetPath,
			bool openAfterDownload)
	{
		auto res = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (res.ID_ == -1)
		{
			auto notif = Util::MakeNotification ("NetStoreManager",
				tr ("Could not find plugin to download %1.")
						.arg (e.Entity_.toString ()),
				PCritical_);
			SendEntity (notif);
			return;
		}

		Id2SavePath_ [res.ID_] = targetPath;
		Id2OpenAfterDownloadState_ [res.ID_] = openAfterDownload;
		HandleProvider (res.Handler_, res.ID_);
	}

	void DownManager::HandleProvider (QObject* provider, int id)
	{
		Id2Downloader_ [id] = provider;

		if (Downloaders_.contains (provider))
			return;

		Downloaders_ << provider;
		connect (provider,
				SIGNAL (jobFinished (int)),
				this,
				SLOT (handleJobFinished (int)));
		connect (provider,
				SIGNAL (jobRemoved (int)),
				this,
				SLOT (handleJobRemoved (int)));
		connect (provider,
				SIGNAL (jobError (int, IDownload::Error)),
				this,
				SLOT (handleJobError (int, IDownload::Error)));

	}

	void DownManager::handleDownloadRequest (const QUrl& url,
			const QString& filePath, TaskParameters tp, bool open)
	{
		QString savePath;
		if (open)
#if QT_VERSION < 0x050000
			savePath = QDesktopServices::storageLocation (QDesktopServices::TempLocation) +
#else
			savePath = QStandardPaths::writableLocation (QStandardPaths::TempLocation) +
#endif
					"/" + filePath;
		else
			savePath = filePath;

		auto e = Util::MakeEntity (url, savePath, tp);
		e.Additional_ ["Filename"] = QFileInfo (filePath).fileName ();
		open ?
			DelegateEntity (e, savePath, open) :
			SendEntity (e);
	}

	void DownManager::handleJobError (int id, IDownload::Error)
	{
		Id2Downloader_.remove (id);
		Id2SavePath_.remove (id);
		Id2OpenAfterDownloadState_.remove (id);
	}

	void DownManager::handleJobFinished (int id)
	{
		QString path = Id2SavePath_.take (id);
		Id2Downloader_.remove (id);

		if (Id2OpenAfterDownloadState_.contains (id) &&
				Id2OpenAfterDownloadState_ [id])
		{
			SendEntity (Util::MakeEntity (QUrl::fromLocalFile (path),
					QString (), OnlyHandle | FromUserInitiated));
			Id2OpenAfterDownloadState_.remove (id);
		}
	}

	void DownManager::handleJobRemoved (int id)
	{
		Id2Downloader_.remove (id);
		Id2SavePath_.remove (id);
		Id2OpenAfterDownloadState_.remove (id);
	}

}
}
