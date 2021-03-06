/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
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

#include "useravatarmanager.h"
#include <QCryptographicHash>
#include <util/threads/futures.h>
#include <interfaces/azoth/iproxyobject.h>
#include "pubsubmanager.h"
#include "useravatardata.h"
#include "useravatarmetadata.h"
#include "clientconnection.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	UserAvatarManager::UserAvatarManager (IAvatarsManager *avatarsMgr, ClientConnection *conn)
	: QObject { conn }
	, Manager_ { conn->GetPubSubManager () }
	, Conn_ { conn }
	, AvatarsMgr_ { avatarsMgr }
	{
		connect (Manager_,
				SIGNAL (gotEvent (QString, PEPEventBase*)),
				this,
				SLOT (handleEvent (QString, PEPEventBase*)));

		Manager_->RegisterCreator<UserAvatarData> ();
		Manager_->RegisterCreator<UserAvatarMetadata> ();
		Manager_->SetAutosubscribe<UserAvatarMetadata> (true);
	}

	void UserAvatarManager::PublishAvatar (const QImage& avatar)
	{
		if (!avatar.isNull ())
		{
			UserAvatarData data (avatar);

			Manager_->PublishEvent (&data);
		}

		UserAvatarMetadata metadata (avatar);
		Manager_->PublishEvent (&metadata);
	}

	void UserAvatarManager::HandleMDEvent (const QString& from, UserAvatarMetadata *mdEvent)
	{
		const auto entry = qobject_cast<ICLEntry*> (Conn_->GetCLEntry (from));
		if (!entry)
			return;

		if (mdEvent->GetID ().isEmpty ())
		{
			emit avatarUpdated (from);
			return;
		}

		const auto& id = mdEvent->GetID ();

		Util::Sequence (this, AvatarsMgr_->GetStoredAvatarData (entry->GetEntryID (), IHaveAvatars::Size::Full)) >>
				[this, id, from] (const boost::optional<QByteArray>& data)
				{
					if (!data || data->isEmpty ())
						return;

					const auto& storedId = QCryptographicHash::hash (*data, QCryptographicHash::Sha1).toHex ();
					if (storedId != id)
						emit avatarUpdated (from);
				};
	}

	void UserAvatarManager::handleEvent (const QString& from, PEPEventBase *event)
	{
		if (auto mdEvent = dynamic_cast<UserAvatarMetadata*> (event))
			HandleMDEvent (from, mdEvent);
	}
}
}
}
