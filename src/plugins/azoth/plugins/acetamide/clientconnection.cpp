/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2011 Oleg Linkin
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

#include "clientconnection.h"
#include <QTextCodec>
#include <util/util.h>
#include <interfaces/azoth/iprotocol.h>
#include <interfaces/azoth/iproxyobject.h>
#include "channelclentry.h"
#include "channelhandler.h"
#include "core.h"
#include "ircprotocol.h"
#include "ircserverclentry.h"
#include "ircserverhandler.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Acetamide
{
	ClientConnection::ClientConnection (IrcAccount *account)
	: Account_ (account)
	, ProxyObject_ (0)
	, IsConsoleEnabled_ (false)
	{
		QObject *proxyObj = qobject_cast<IrcProtocol*> (account->
				GetParentProtocol ())->GetProxyObject ();
		ProxyObject_ = qobject_cast<IProxyObject*> (proxyObj);
	}

	void ClientConnection::Sinchronize ()
	{
	}

	IrcAccount* ClientConnection::GetAccount () const
	{
		return Account_;
	}

	QList<IrcServerHandler*> ClientConnection::GetServerHandlers () const
	{
		return ServerHandlers_.values ();
	}

	bool ClientConnection::IsServerExists (const QString& key)
	{
		return ServerHandlers_.contains (key);
	}

	void ClientConnection::JoinServer (const ServerOptions& server)
	{
		QString serverId = server.ServerName_ + ":" +
				QString::number (server.ServerPort_);

		IrcServerHandler *ish = new IrcServerHandler (server, Account_);
		emit gotRosterItems (QList<QObject*> () << ish->GetCLEntry ());
		connect (ish,
				SIGNAL (gotSocketError (QAbstractSocket::SocketError, const QString&)),
				this,
				SLOT (handleError(QAbstractSocket::SocketError, const QString&)));

		ish->SetConsoleEnabled (IsConsoleEnabled_);
		if (IsConsoleEnabled_)
			connect (ish,
					SIGNAL (sendMessageToConsole (IMessage::Direction, const QString&)),
					this,
					SLOT (handleLog (IMessage::Direction, const QString&)),
					Qt::UniqueConnection);
		else
			disconnect (ish,
					SIGNAL (sendMessageToConsole (IMessage::Direction, const QString&)),
					this,
					SLOT (handleLog (IMessage::Direction, const QString&)));
		ServerHandlers_ [serverId] = ish;

		ish->ConnectToServer ();
	}

	void  ClientConnection::JoinChannel (const ServerOptions& server,
			const ChannelOptions& channel)
	{
		QString serverId = server.ServerName_ + ":" +
				QString::number (server.ServerPort_);
		QString channelId = channel.ChannelName_ + "@" +
				channel.ServerName_;

		if (ServerHandlers_ [serverId]->IsChannelExists (channelId))
		{
			Entity e = Util::MakeNotification ("Azoth",
				tr ("This channel is already joined."),
				PCritical_);
			Core::Instance ().SendEntity (e);
			return;
		}

		if (!channel.ChannelName_.isEmpty ())
			ServerHandlers_ [serverId]->JoinChannel (channel);
	}

	void ClientConnection::SetBookmarks (const QList<IrcBookmark>& bookmarks)
	{
		QList<QVariant> res;
		Q_FOREACH (const IrcBookmark& bookmark, bookmarks)
		{
			QByteArray result;
			{
				QDataStream ostr (&result, QIODevice::WriteOnly);
				ostr << static_cast<quint8> (1)
						<< bookmark.Name_
						<< bookmark.ServerName_
						<< bookmark.ServerPort_
						<< bookmark.ServerPassword_
						<< bookmark.ServerEncoding_
						<< bookmark.ChannelName_
						<< bookmark.ChannelPassword_
						<< bookmark.NickName_
						<< bookmark.SSL_
						<< bookmark.AutoJoin_;
			}

			res << QVariant::fromValue (result);
		}
		XmlSettingsManager::Instance ().setProperty ("Bookmarks",
				QVariant::fromValue (res));
	}

	QList<IrcBookmark> ClientConnection::GetBookmarks () const
	{
		QList<QVariant> list = XmlSettingsManager::Instance ().Property ("Bookmarks",
				QList<QVariant> ()).toList ();

		bool hadUnknownVersions = false;

		QList<IrcBookmark> bookmarks;
		Q_FOREACH (const QVariant& variant, list)
		{
			IrcBookmark bookmark;
			QDataStream istr (variant.toByteArray ());

			quint8 version = 0;
			istr >> version;
			if (version != 1)
			{
				qWarning () << Q_FUNC_INFO
						<< "unknown version"
						<< version;
				hadUnknownVersions = true;
				continue;
			}

			istr >> bookmark.Name_
					>> bookmark.ServerName_
					>> bookmark.ServerPort_
					>> bookmark.ServerPassword_
					>> bookmark.ServerEncoding_
					>> bookmark.ChannelName_
					>> bookmark.ChannelPassword_
					>> bookmark.NickName_
					>> bookmark.SSL_
					>> bookmark.AutoJoin_;

			bookmarks << bookmark;
		}

		if (hadUnknownVersions)
			Core::Instance ().SendEntity (Util::MakeNotification ("Azoth Acetamide",
						tr ("Some bookmarks were lost due to unknown storage version."),
						PWarning_));

		return bookmarks;
	}

	IrcServerHandler* ClientConnection::GetIrcServerHandler (const QString& id) const
	{
		return ServerHandlers_ [id];
	}

	void ClientConnection::DisconnectFromAll ()
	{
		Q_FOREACH (auto ish, ServerHandlers_.values ())
			ish->SendQuit ();
	}

	void ClientConnection::QuitServer (const QStringList& list)
	{
		auto ish = ServerHandlers_ [list.last ()];
		ish->DisconnectFromServer ();
	}

	void ClientConnection::SetConsoleEnabled (bool enabled)
	{
		IsConsoleEnabled_ = enabled;
		Q_FOREACH (auto srv, ServerHandlers_.values ())
		{
			srv->SetConsoleEnabled (enabled);
			if (enabled)
				connect (srv,
						SIGNAL (sendMessageToConsole (IMessage::Direction, const QString&)),
						this,
						SLOT (handleLog (IMessage::Direction, const QString&)),
						Qt::UniqueConnection);
			else
				disconnect (srv,
						SIGNAL (sendMessageToConsole (IMessage::Direction, const QString&)),
						this,
						SLOT (handleLog (IMessage::Direction, const QString&)));
		}
	}

	void ClientConnection::ClosePrivateChat (const QString& serverID, QString nick)
	{
		if (ServerHandlers_.contains (serverID))
			ServerHandlers_ [serverID]->ClosePrivateChat (nick);
	}

	void ClientConnection::FetchVCard (const QString& serverId, const QString& nick)
	{
		if (!ServerHandlers_.contains (serverId))
			return;

		ServerHandlers_ [serverId]->VCardRequest (nick);
	}

	void ClientConnection::SetAway (bool away, const QString& message)
	{
		QString msg = message;
		if (msg.isEmpty ())
			msg = GetStatusStringForState (SAway);

		if (!away)
			msg.clear ();

		QList<IrcServerHandler*> handlers = ServerHandlers_.values ();
		std::for_each (handlers.begin (), handlers.end (),
				[msg] (decltype (handlers.front ()) handler)
				{
					handler->SetAway (msg);
				});
	}

	QString ClientConnection::GetStatusStringForState (State state)
	{
		const QString& statusKey = "DefaultStatus" + QString::number (state);
		return ProxyObject_->GetSettingsManager ()->
				property (statusKey.toUtf8 ()).toString ();
	}

	void ClientConnection::serverConnected (const QString&)
	{
		if (Account_->GetState ().State_ == SOffline)
		{
			Account_->ChangeState (EntryStatus (SOnline, QString ()));
			Account_->SetState (EntryStatus (SOnline, QString ()));
		}
	}

	void ClientConnection::serverDisconnected (const QString& serverId)
	{
		if (!ServerHandlers_.contains (serverId))
			return;

		ServerHandlers_ [serverId]->DisconnectFromServer ();
		Account_->handleEntryRemoved (ServerHandlers_ [serverId]->
				GetCLEntry ());
		ServerHandlers_.take (serverId)->deleteLater ();
		if (!ServerHandlers_.count ())
			Account_->SetState (EntryStatus (SOffline,
					QString ()));
	}

	void ClientConnection::handleError (QAbstractSocket::SocketError error,
			const QString& errorString)
	{
		qWarning () << Q_FUNC_INFO
				<< error
				<< errorString;
		IrcServerHandler *ish = qobject_cast<IrcServerHandler*> (sender ());
		if (!ish)
		{
			qWarning () << Q_FUNC_INFO
					<< "is not an IrcServerHandler"
					<< sender ();
			return;
		}

		Entity e = Util::MakeNotification ("Azoth",
				errorString,
				PCritical_);
		Core::Instance ().SendEntity (e);

		QList<ChannelOptions> activeChannels;
		const auto& channelHandlers = ish->GetChannelHandlers ();
		std::transform (channelHandlers.begin (), channelHandlers.end (),
				std::back_inserter (activeChannels),
				[] (decltype (channelHandlers.first ()) handler)
					{ return handler->GetChannelOptions (); });

		for (const auto& co : activeChannels)
			JoinChannel (ish->GetServerOptions (), co);
	}

	void ClientConnection::handleLog (IMessage::Direction type, const QString& msg)
	{
		switch (type)
		{
		case IMessage::DOut:
			emit gotConsoleLog (msg.toUtf8 (), IHaveConsole::PacketDirection::Out, {});
			break;
		case IMessage::DIn:
			emit gotConsoleLog (msg.toUtf8 (), IHaveConsole::PacketDirection::In, {});
			break;
		default:
			break;
		}
	}

};
};
};
