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

#include "account.h"
#include <QtDebug>
#include <QTimer>
#include <util/xpc/passutils.h>
#include <util/xpc/util.h>
#include <interfaces/core/ientitymanager.h>
#include "protocol.h"
#include "util.h"
#include "buddy.h"
#include "accountconfigdialog.h"

namespace LeechCraft
{
namespace Azoth
{
namespace VelvetBird
{
	Account::Account (PurpleAccount *acc, Protocol *proto)
	: QObject (proto)
	, Account_ (acc)
	, Proto_ (proto)
	{
		UpdateStatus ();
	}

	void Account::Release ()
	{
		emit removedCLItems (GetCLEntries ());
		qDeleteAll (Buddies_);
		Buddies_.clear ();
	}

	PurpleAccount* Account::GetPurpleAcc () const
	{
		return Account_;
	}

	QObject* Account::GetQObject ()
	{
		return this;
	}

	QObject* Account::GetParentProtocol () const
	{
		return Proto_;
	}

	IAccount::AccountFeatures Account::GetAccountFeatures () const
	{
		return FRenamable;
	}

	QList<QObject*> Account::GetCLEntries ()
	{
		QList<QObject*> result;
		for (auto buddy : Buddies_)
			result << buddy;
		return result;
	}

	QString Account::GetAccountName () const
	{
		return QString::fromUtf8 (purple_account_get_string (Account_, "AccountName", ""));
	}

	QString Account::GetOurNick () const
	{
		return QString::fromUtf8 (purple_account_get_name_for_display (Account_));
	}

	void Account::RenameAccount (const QString& name)
	{
		purple_account_set_string (Account_, "AccountName", name.toUtf8 ().constData ());
		emit accountRenamed (name);
	}

	QByteArray Account::GetAccountID () const
	{
		return Proto_->GetProtocolID () + "_" + purple_account_get_username (Account_);
	}

	QList<QAction*> Account::GetActions () const
	{
		return {};
	}

	void Account::QueryInfo (const QString&)
	{
	}

	void Account::OpenConfigurationDialog ()
	{
		AccountConfigDialog dia;

		const auto& curAlias = QString::fromUtf8 (purple_account_get_alias (Account_));
		dia.SetNick (curAlias);

		const auto& curUser = QString::fromUtf8 (purple_account_get_username (Account_));
		dia.SetUser (curUser);

		if (dia.exec () != QDialog::Accepted)
			return;

		if (curAlias != dia.GetNick ())
			purple_account_set_alias (Account_, dia.GetNick ().toUtf8 ().constData ());

		if (curUser != dia.GetUser ())
			purple_account_set_username (Account_, dia.GetUser ().toUtf8 ().constData ());
	}

	EntryStatus Account::GetState () const
	{
		return CurrentStatus_;
	}

	void Account::ChangeState (const EntryStatus& status)
	{
		if (status.State_ != SOffline && !purple_account_get_password (Account_))
		{
			const auto& str = Util::GetPassword ("Azoth." + GetAccountID (),
					tr ("Enter password for account %1:").arg (GetAccountName ()),
					Proto_->GetCoreProxy (),
					true);
			if (str.isEmpty ())
				return;

			purple_account_set_password (Account_, str.toUtf8 ().constData ());
		}

		if (!purple_account_get_enabled (Account_, "leechcraft.azoth"))
			purple_account_set_enabled (Account_, "leechcraft.azoth", true);

		auto type = purple_account_get_status_type_with_primitive (Account_, ToPurpleState (status.State_));
		auto statusId = type ? purple_status_type_get_id (type) : "available";
		purple_account_set_status (Account_,
				statusId,
				true,
				"message",
				status.StatusString_.toUtf8 ().constData (),
				NULL);
	}

	void Account::Authorize (QObject*)
	{
	}

	void Account::DenyAuth (QObject*)
	{
	}

	void Account::RequestAuth (const QString& entry,
			const QString& msg, const QString& name, const QStringList&)
	{
		auto buddy = purple_buddy_new (Account_,
				entry.toUtf8 ().constData (),
				name.isEmpty () ? nullptr : name.toUtf8 ().constData ());
		purple_blist_add_buddy (buddy, NULL, NULL, NULL);
		purple_account_add_buddy_with_invite (Account_, buddy, msg.toUtf8 ().constData ());
	}

	void Account::RemoveEntry (QObject *entryObj)
	{
		auto buddy = qobject_cast<Buddy*> (entryObj);
		purple_account_remove_buddy (Account_, buddy->GetPurpleBuddy (), NULL);
		purple_blist_remove_buddy (buddy->GetPurpleBuddy ());
	}

	QObject* Account::GetTransferManager () const
	{
		return 0;
	}

	void Account::UpdateBuddy (PurpleBuddy *purpleBuddy)
	{
		if (!Buddies_.contains (purpleBuddy))
		{
			auto buddy = new Buddy (purpleBuddy, this);
			Buddies_ [purpleBuddy] = buddy;
			emit gotCLItems ({ buddy });
		}

		Buddies_ [purpleBuddy]->Update ();
	}

	void Account::RemoveBuddy (PurpleBuddy *purpleBuddy)
	{
		auto buddy = Buddies_.take (purpleBuddy);
		if (!buddy)
			return;

		emit removedCLItems ({ buddy });
		delete buddy;
	}

	void Account::HandleDisconnect (PurpleConnectionError error, const char *text)
	{
		const auto& prevStatus = GetState ();

		CurrentStatus_ = EntryStatus ();
		emit statusChanged (CurrentStatus_);

		const auto& eNotify = Util::MakeNotification (GetAccountName (),
				tr ("Connection error: %1.")
					.arg (QString::fromUtf8 (text)),
				PCritical_);
		Proto_->GetCoreProxy ()->GetEntityManager ()->HandleEntity (eNotify);

		if (error == PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED)
			QMetaObject::invokeMethod (this,
					"handleAuthFailure",
					Qt::QueuedConnection,
					Q_ARG (LeechCraft::Azoth::EntryStatus, prevStatus));
	}

	void Account::HandleConvLessMessage (PurpleConversation *conv,
			const char *who, const char *message, PurpleMessageFlags flags, time_t mtime)
	{
		for (auto buddy : Buddies_)
		{
			if (buddy->GetHumanReadableID () != who)
				continue;

			buddy->SetConv (conv);
			buddy->HandleMessage (who, message, flags, mtime);
			break;
		}
	}

	void Account::UpdateStatus ()
	{
		HandleStatus (purple_account_get_active_status (Account_));
	}

	void Account::HandleStatus (PurpleStatus *status)
	{
		CurrentStatus_ = status ? FromPurpleStatus (Account_, status) : EntryStatus ();
		qDebug () << Q_FUNC_INFO << CurrentStatus_.State_;
		emit statusChanged (CurrentStatus_);

		QTimer::singleShot (5000, this, SLOT (updateIcon ()));
	}

	void Account::updateIcon ()
	{
		auto img = purple_buddy_icons_find_account_icon (Account_);
		if (!img)
			return;

		const auto data = purple_imgstore_get_data (img);
		const auto size = purple_imgstore_get_size (img);

		auto image = QImage::fromData (reinterpret_cast<const uchar*> (data), size);
		qDebug () << Q_FUNC_INFO << image.isNull ();

		purple_imgstore_unref (img);
	}

	void Account::handleAuthFailure (const EntryStatus& prevStatus)
	{
		ChangeState (EntryStatus ());
		const auto& str = Util::GetPassword ("Azoth." + GetAccountID (),
				tr ("Enter password for account %1:").arg (GetAccountName ()),
				Proto_->GetCoreProxy (),
				false);
		if (str.isEmpty ())
			return;

		purple_account_set_password (Account_, str.toUtf8 ().constData ());

		ChangeState (prevStatus);

		HandleStatus (purple_account_get_active_status (Account_));
	}
}
}
}
