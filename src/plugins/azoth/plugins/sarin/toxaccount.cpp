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

#include "toxaccount.h"
#include <QUuid>
#include <QAction>
#include <QDataStream>
#include <QFutureWatcher>
#include <QtDebug>
#include <tox/tox.h>
#include <util/xpc/util.h>
#include <util/sll/slotclosure.h>
#include <interfaces/core/ientitymanager.h>
#include "toxprotocol.h"
#include "toxthread.h"
#include "showtoxiddialog.h"
#include "toxcontact.h"
#include "messagesmanager.h"
#include "chatmessage.h"
#include "accountconfigdialog.h"
#include "util.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	ToxAccount::ToxAccount (const QByteArray& uid, const QString& name, ToxProtocol *parent)
	: QObject { parent }
	, Proto_ { parent }
	, UID_ { uid }
	, Name_ { name }
	, ActionGetToxId_ { new QAction { tr ("Get Tox ID"), this } }
	, MsgsMgr_ { new MessagesManager { this } }
	{
		connect (ActionGetToxId_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleToxIdRequested ()));

		connect (MsgsMgr_,
				SIGNAL (gotMessage (QByteArray, QString)),
				this,
				SLOT (handleInMessage (QByteArray, QString)));
	}

	ToxAccount::ToxAccount (const QString& name, ToxProtocol *parent)
	: ToxAccount { QUuid::createUuid ().toByteArray (), name, parent }
	{
	}

	QByteArray ToxAccount::Serialize ()
	{
		QByteArray ba;
		QDataStream str { &ba, QIODevice::WriteOnly };
		str << static_cast<quint8> (2)
				<< UID_
				<< Name_
				<< Nick_
				<< ToxState_
				<< ToxConfig_;

		return ba;
	}

	ToxAccount* ToxAccount::Deserialize (const QByteArray& data, ToxProtocol *proto)
	{
		QDataStream str { data };
		quint8 version = 0;
		str >> version;
		if (version < 1 || version > 2)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< version;
			return nullptr;
		}

		QByteArray uid;
		QString name;
		str >> uid
				>> name;

		const auto acc = new ToxAccount { uid, name, proto };
		str >> acc->Nick_
				>> acc->ToxState_;

		if (version >= 2)
			str >> acc->ToxConfig_;

		return acc;
	}

	void ToxAccount::SetNickname (const QString& nickname)
	{
		if (nickname == Nick_)
			return;

		Nick_ = nickname;
		emit accountChanged (this);
	}

	QObject* ToxAccount::GetQObject ()
	{
		return this;
	}

	QObject* ToxAccount::GetParentProtocol () const
	{
		return Proto_;
	}

	IAccount::AccountFeatures ToxAccount::GetAccountFeatures () const
	{
		return FRenamable;
	}

	QList<QObject*> ToxAccount::GetCLEntries ()
	{
		QList<QObject*> result;
		std::copy (Contacts_.begin (), Contacts_.end (), std::back_inserter (result));
		return result;
	}

	QString ToxAccount::GetAccountName () const
	{
		return Name_;
	}

	QString ToxAccount::GetOurNick () const
	{
		return Nick_.isEmpty () ? Name_ : Nick_;
	}

	void ToxAccount::RenameAccount (const QString& name)
	{
		if (name == Name_)
			return;

		Name_ = name;
		emit accountRenamed (Name_);
		emit accountChanged (this);
	}

	QByteArray ToxAccount::GetAccountID () const
	{
		return UID_;
	}

	QList<QAction*> ToxAccount::GetActions () const
	{
		return { ActionGetToxId_ };
	}

	void ToxAccount::OpenConfigurationDialog ()
	{
		auto configDialog = new AccountConfigDialog;
		configDialog->setAttribute (Qt::WA_DeleteOnClose);
		configDialog->show ();

		configDialog->SetConfig (ToxConfig_);

		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, configDialog] { HandleConfigAccepted (configDialog); },
			configDialog,
			SIGNAL (accepted ()),
			configDialog
		};
	}

	EntryStatus ToxAccount::GetState () const
	{
		return Thread_ ? Thread_->GetStatus () : EntryStatus {};
	}

	void ToxAccount::ChangeState (const EntryStatus& status)
	{
		if (status.State_ == SOffline)
		{
			if (Thread_ && Thread_->IsStoppable ())
			{
				new Util::SlotClosure<Util::DeleteLaterPolicy>
				{
					[status, guard = Thread_, this] { emit statusChanged (status); },
					Thread_.get (),
					SIGNAL (finished ()),
					Thread_.get ()
				};
				Thread_->Stop ();
			}
			Thread_.reset ();
			emit threadChanged (Thread_);
			return;
		}

		if (!Thread_)
			InitThread (status);
		else
			Thread_->SetStatus (status);
	}

	void ToxAccount::Authorize (QObject *entryObj)
	{
		if (!Thread_)
			return;

		const auto entry = qobject_cast<ToxContact*> (entryObj);
		const auto& toxId = entry->GetFriendId ().toLatin1 ();
		if (toxId.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty friend ID for"
					<< entry->GetHumanReadableID ();
			const auto& e = Util::MakeNotification ("Azoth Sarin",
					tr ("Unable to authorize friend: unknown friend ID."),
					PWarning_);
			Proto_->GetCoreProxy ()->GetEntityManager ()->HandleEntity (e);
			return;
		}

		Thread_->AddFriend (toxId);
	}

	void ToxAccount::DenyAuth (QObject*)
	{
	}

	namespace
	{
		void HandleAddFriendResult (ToxThread::AddFriendResult result, const ICoreProxy_ptr& proxy)
		{
			const auto notify = [proxy] (Priority prio, const QString& text)
			{
				const auto& e = Util::MakeNotification ("Azoth Sarin", text, prio);
				proxy->GetEntityManager ()->HandleEntity (e);
			};

			switch (result)
			{
			case ToxThread::AddFriendResult::Added:
				return;
			case ToxThread::AddFriendResult::InvalidId:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: invalid Tox ID."));
				return;
			case ToxThread::AddFriendResult::TooLong:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: too long greeting message."));
				return;
			case ToxThread::AddFriendResult::NoMessage:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: no message."));
				return;
			case ToxThread::AddFriendResult::OwnKey:
				notify (PCritical_, ToxAccount::tr ("Why would you add yourself as friend?"));
				return;
			case ToxThread::AddFriendResult::AlreadySent:
				notify (PWarning_, ToxAccount::tr ("Friend request has already been sent."));
				return;
			case ToxThread::AddFriendResult::BadChecksum:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: bad Tox ID checksum."));
				return;
			case ToxThread::AddFriendResult::NoSpam:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: nospam value has been changed. Get a freshier ID!"));
				return;
			case ToxThread::AddFriendResult::NoMem:
				notify (PCritical_, ToxAccount::tr ("Friend was not added: no memory (but how do you see this in that case?)."));
				return;
			case ToxThread::AddFriendResult::Unknown:
				notify (PCritical_, ToxAccount::tr ("Friend was not added because of some unknown error."));
				return;
			}
		}
	}

	void ToxAccount::RequestAuth (const QString& toxId, const QString& msg, const QString&, const QStringList&)
	{
		if (!Thread_)
			return;

		const auto watcher = new QFutureWatcher<ToxThread::AddFriendResult>;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[watcher, this]
			{
				HandleAddFriendResult (watcher->result (), Proto_->GetCoreProxy ());
				watcher->deleteLater ();
			},
			watcher,
			SIGNAL (finished ()),
			watcher
		};
		watcher->setFuture (Thread_->AddFriend (toxId.toLatin1 (), msg));
	}

	void ToxAccount::RemoveEntry (QObject *entryObj)
	{
		if (!Thread_)
			return;

		const auto entry = qobject_cast<ToxContact*> (entryObj);
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< entryObj
					<< "is not a ToxContact";
			return;
		}

		Thread_->RemoveFriend (entry->GetHumanReadableID ().toUtf8 ());
	}

	QObject* ToxAccount::GetTransferManager () const
	{
		return nullptr;
	}

	void ToxAccount::SendMessage (const QByteArray& pkey, ChatMessage *message)
	{
		MsgsMgr_->SendMessage (pkey, message);
	}

	void ToxAccount::SetTypingState (const QByteArray& pkey, bool isTyping)
	{
		if (!Thread_)
			return;

		Thread_->ScheduleFunction ([pkey, isTyping] (Tox *tox)
				{
					const auto num = GetFriendId (tox, pkey);
					if (num < 0)
					{
						qWarning () << Q_FUNC_INFO
								<< "unable to find user ID for"
								<< pkey;
						return;
					}
					tox_set_user_is_typing (tox, num, isTyping);
				});
	}

	void ToxAccount::InitThread (const EntryStatus& status)
	{
		Thread_ = std::make_shared<ToxThread> (Nick_, ToxState_, ToxConfig_);
		Thread_->SetStatus (status);
		connect (Thread_.get (),
				SIGNAL (statusChanged (EntryStatus)),
				this,
				SIGNAL (statusChanged (EntryStatus)));
		connect (Thread_.get (),
				SIGNAL (toxStateChanged (QByteArray)),
				this,
				SLOT (handleToxStateChanged (QByteArray)));
		connect (Thread_.get (),
				SIGNAL (gotFriendRequest (QByteArray, QByteArray, QString)),
				this,
				SLOT (handleGotFriendRequest (QByteArray, QByteArray, QString)));
		connect (Thread_.get (),
				SIGNAL (gotFriend (qint32)),
				this,
				SLOT (handleGotFriend (qint32)));
		connect (Thread_.get (),
				SIGNAL (friendNameChanged (QByteArray, QString)),
				this,
				SLOT (handleFriendNameChanged (QByteArray, QString)));
		connect (Thread_.get (),
				SIGNAL (friendStatusChanged (QByteArray, EntryStatus)),
				this,
				SLOT (handleFriendStatusChanged (QByteArray, EntryStatus)));
		connect (Thread_.get (),
				SIGNAL (friendTypingChanged (QByteArray, bool)),
				this,
				SLOT (handleFriendTypingChanged (QByteArray, bool)));
		connect (Thread_.get (),
				SIGNAL (removedFriend (QByteArray)),
				this,
				SLOT (handleRemovedFriend (QByteArray)));

		emit threadChanged (Thread_);

		Thread_->start (QThread::IdlePriority);
	}

	void ToxAccount::InitEntry (const QByteArray& pkey)
	{
		const auto entry = new ToxContact { pkey, this };
		Contacts_ [pkey] = entry;

		emit gotCLItems ({ entry });
	}

	void ToxAccount::HandleConfigAccepted (AccountConfigDialog *dialog)
	{
		const auto& config = dialog->GetConfig ();
		if (config == ToxConfig_)
			return;

		ToxConfig_ = config;
		emit accountChanged (this);
	}

	void ToxAccount::handleToxIdRequested ()
	{
		if (!Thread_)
			return;

		auto dialog = new ShowToxIdDialog { tr ("Fetching self Tox ID...") };
		dialog->show ();
		dialog->setAttribute (Qt::WA_DeleteOnClose);

		auto watcher = new QFutureWatcher<QByteArray>;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[watcher, dialog]
			{
				const auto& res = watcher->result ();
				dialog->setToxId (QString::fromLatin1 (res));
				watcher->deleteLater ();
			},
			watcher,
			SIGNAL (finished ()),
			watcher
		};
		watcher->setFuture (Thread_->GetToxId ());
	}

	void ToxAccount::handleToxStateChanged (const QByteArray& toxState)
	{
		ToxState_ = toxState;
		emit accountChanged (this);
	}

	void ToxAccount::handleGotFriend (qint32 id)
	{
		const auto watcher = new QFutureWatcher<ToxThread::FriendInfo>;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[watcher, this]
			{
				watcher->deleteLater ();

				try
				{
					const auto& info = watcher->result ();
					qDebug () << Q_FUNC_INFO << info.Pubkey_ << info.Name_;
					if (!Contacts_.contains (info.Pubkey_))
						InitEntry (info.Pubkey_);

					const auto entry = Contacts_.value (info.Pubkey_);
					entry->SetEntryName (info.Name_);

					entry->SetStatus (info.Status_);
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot get friend info:"
							<< e.what ();
				}
			},
			watcher,
			SIGNAL (finished ()),
			watcher
		};

		watcher->setFuture (Thread_->ResolveFriend (id));
	}

	void ToxAccount::handleGotFriendRequest (const QByteArray& friendId, const QByteArray& pubkey, const QString& msg)
	{
		if (!Contacts_.contains (pubkey))
			InitEntry (pubkey);

		const auto contact = Contacts_.value (pubkey);
		contact->SetFriendId (friendId);

		emit authorizationRequested (Contacts_.value (pubkey), msg.trimmed ());
	}

	void ToxAccount::handleRemovedFriend (const QByteArray& pubkey)
	{
		if (!Contacts_.contains (pubkey))
			return;

		const auto item = Contacts_.take (pubkey);
		emit removedCLItems ({ item });
		item->deleteLater ();
	}

	void ToxAccount::handleFriendNameChanged (const QByteArray& id, const QString& newName)
	{
		if (!Contacts_.contains (id))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown friend name change"
					<< id
					<< "; new name:"
					<< newName;
			return;
		}

		Contacts_.value (id)->SetEntryName (newName);
	}

	void ToxAccount::handleFriendStatusChanged (const QByteArray& pubkey, const EntryStatus& status)
	{
		if (!Contacts_.contains (pubkey))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown friend status"
					<< pubkey;
			return;
		}

		Contacts_.value (pubkey)->SetStatus (status);
	}

	void ToxAccount::handleFriendTypingChanged (const QByteArray& pubkey, bool isTyping)
	{
		if (!Contacts_.contains (pubkey))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown friend status"
					<< pubkey;
			return;
		}

		Contacts_.value (pubkey)->SetTyping (isTyping);
	}

	void ToxAccount::handleInMessage (const QByteArray& pubkey, const QString& body)
	{
		if (!Contacts_.contains (pubkey))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown pubkey for message"
					<< body
					<< pubkey;
			InitEntry (pubkey);
		}

		const auto contact = Contacts_.value (pubkey);
		const auto msg = new ChatMessage { body, IMessage::Direction::In, contact };
		msg->Store ();
	}
}
}
}
