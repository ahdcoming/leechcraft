/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2013  Oleg Linkin <MaledictusDeMagog@gmail.com>
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

#pragma once

#include <functional>
#include <memory>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QQueue>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/blasq/iaccount.h>
#include <interfaces/blasq/isupportuploads.h>
#include "structures.h"

class QStandardItemModel;
class QStandardItem;

namespace LeechCraft
{
namespace Blasq
{
namespace DeathNote
{
	class FotoBilderService;

	class FotoBilderAccount : public QObject
							, public IAccount
							, public ISupportUploads
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Blasq::IAccount LeechCraft::Blasq::ISupportUploads)

		QString Name_;
		FotoBilderService * const Service_;
		const ICoreProxy_ptr Proxy_;
		QByteArray ID_;
		QString Login_;
		bool FirstRequest_;
		Quota Quota_;

		QStandardItemModel * const CollectionsModel_;
		QStandardItem *AllPhotosItem_;

		QHash<QByteArray, QStandardItem*> Id2AlbumItem_;
		QHash<QNetworkReply*, UploadItem> Reply2UploadItem_;

		QQueue<std::function <void (const QString&)>> CallsQueue_;
	public:
		FotoBilderAccount (const QString& name, FotoBilderService *service,
				ICoreProxy_ptr proxy, const QString& login,
				const QByteArray& id = QByteArray ());

		ICoreProxy_ptr GetProxy () const;

		QByteArray Serialize () const;
		static FotoBilderAccount* Deserialize (const QByteArray& data,
				FotoBilderService *service, ICoreProxy_ptr proxy);

		QObject* GetQObject () override;
		IService* GetService () const override;
		QString GetName () const override;
		QByteArray GetID () const override;
		QString GetPassword () const;

		QAbstractItemModel* GetCollectionsModel () const override;

		void CreateCollection (const QModelIndex& parent) override;
		bool HasUploadFeature (Feature) const override;
		void UploadImages (const QModelIndex& collection, const QList<UploadItem>& paths) override;

		void Login ();
		void RequestGalleries ();
		void RequestPictures ();

		void UpdateCollections () override;

	private:
		std::shared_ptr<void> MakeRunnerGuard ();
		void CallNextFunctionFromQueue ();
		bool IsErrorReply (const QByteArray& content);
		void GetChallenge ();
		void LoginRequest (const QString& challenge);
		void GetGalsRequest (const QString& challenge);
		void GetPicsRequest (const QString& challenge);
		void CreateGallery (const QString& name, int privacyLevel, const QString& challenge);
		void UploadImagesRequest (const QByteArray& albumId, const QList<UploadItem>& items);
		void UploadOneImage (const QByteArray& id,
				const UploadItem& item, const QString& challenge);

	private slots:
		void handleGetChallengeRequestFinished ();
		void handleLoginRequestFinished ();

		void handleNetworkError (QNetworkReply::NetworkError err);

		void handleGotAlbums ();
		void handleGotPhotos ();
		void handleGalleryCreated ();
		void handleUploadProgress (qint64 sent, qint64 total);
		void handleImageUploaded ();

	signals:
		void accountChanged (FotoBilderAccount *acc);
		void doneUpdating () override;
		void networkError (QNetworkReply::NetworkError err, const QString& errString);
		void itemUploaded (const UploadItem&, const QUrl&) override;
	};
}
}
}
