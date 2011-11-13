/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "accountthreadworker.h"
#include <QMutexLocker>
#include <QtDebug>
#include <vmime/security/defaultAuthenticator.hpp>
#include <vmime/security/cert/defaultCertificateVerifier.hpp>
#include <vmime/security/cert/X509Certificate.hpp>
#include <vmime/net/transport.hpp>
#include <vmime/net/store.hpp>
#include <vmime/net/message.hpp>
#include <vmime/utility/datetimeUtils.hpp>
#include <vmime/dateTime.hpp>
#include <vmime/charsetConverter.hpp>
#include <vmime/messageParser.hpp>
#include <vmime/htmlTextPart.hpp>
#include "message.h"
#include "account.h"
#include "core.h"
#include "progresslistener.h"
#include "storage.h"

namespace LeechCraft
{
namespace Snails
{
	namespace
	{
		class VMimeAuth : public vmime::security::defaultAuthenticator
		{
			Account::Direction Dir_;
			Account *Acc_;
		public:
			VMimeAuth (Account::Direction, Account*);

			const vmime::string getUsername () const;
			const vmime::string getPassword () const;
		private:
			QByteArray GetID () const
			{
				QByteArray id = "org.LeechCraft.Snails.PassForAccount/" + Acc_->GetID ();
				id += Dir_ == Account::Direction::Out ? "/Out" : "/In";
				return id;
			}
		};

		VMimeAuth::VMimeAuth (Account::Direction dir, Account *acc)
		: Dir_ (dir)
		, Acc_ (acc)
		{
		}

		const vmime::string VMimeAuth::getUsername () const
		{
			switch (Dir_)
			{
			case Account::Direction::Out:
				return Acc_->GetOutUsername ().toUtf8 ().constData ();
			default:
				return Acc_->GetInUsername ().toUtf8 ().constData ();
			}
		}

		const vmime::string VMimeAuth::getPassword () const
		{
			QString pass;

			QMetaObject::invokeMethod (Acc_,
					"getPassword",
					Qt::BlockingQueuedConnection,
					Q_ARG (QString*, &pass));

			return pass.toUtf8 ().constData ();
		}

		class CertVerifier : public vmime::security::cert::defaultCertificateVerifier
		{
			static std::vector<vmime::ref<vmime::security::cert::X509Certificate>> TrustedCerts_;
		public:
			void verify (vmime::ref<vmime::security::cert::certificateChain> chain)
			{
				try
				{
					setX509TrustedCerts (TrustedCerts_);

					defaultCertificateVerifier::verify (chain);
				}
				catch (vmime::exceptions::certificate_verification_exception&)
				{
					auto cert = chain->getAt (0);

					if (cert->getType() == "X.509")
						TrustedCerts_.push_back (cert.dynamicCast
								<vmime::security::cert::X509Certificate>());
				}
			}
		};

		std::vector<vmime::ref<vmime::security::cert::X509Certificate>> CertVerifier::TrustedCerts_;
	}

	AccountThreadWorker::AccountThreadWorker (Account *parent)
	: A_ (parent)
	{
		Session_ = vmime::create<vmime::net::session> ();
	}

	vmime::utility::ref<vmime::net::store> AccountThreadWorker::MakeStore ()
	{
		QString url;

		QMetaObject::invokeMethod (A_,
				"buildInURL",
				Qt::BlockingQueuedConnection,
				Q_ARG (QString*, &url));

		auto st = Session_->getStore (vmime::utility::url (url.toUtf8 ().constData ()));
		st->setCertificateVerifier (vmime::create<CertVerifier> ());

		qDebug () << Q_FUNC_INFO << A_->UseTLS_;
		st->setProperty ("connection.tls", A_->UseTLS_);
		st->setProperty ("connection.tls.required", A_->TLSRequired_);
		st->setProperty ("options.sasl", A_->UseSASL_);
		st->setProperty ("options.sasl.fallback", A_->SASLRequired_);

		return st;
	}

	vmime::utility::ref<vmime::net::transport> AccountThreadWorker::MakeTransport ()
	{
		return Session_->getTransport (vmime::utility::url (A_->BuildOutURL ().toUtf8 ().constData ()));
	}

	Message_ptr AccountThreadWorker::FromHeaders (const vmime::ref<vmime::net::message>& message) const
	{
		auto utf8cs = vmime::charset ("utf-8");

		Message_ptr msg (new Message);
		msg->SetID (message->getUniqueId ().c_str ());
		msg->SetSize (message->getSize ());

		if (message->getFlags () & vmime::net::message::FLAG_SEEN)
			msg->SetRead (true);

		auto header = message->getHeader ();

		try
		{
			auto mbox = header->From ()->getValue ().dynamicCast<const vmime::mailbox> ();
			msg->SetFromEmail (QString::fromUtf8 (mbox->getEmail ().c_str ()));
			msg->SetFrom (QString::fromUtf8 (mbox->getName ().getConvertedText (utf8cs).c_str ()));
		}
		catch (const vmime::exceptions::no_such_field& nsf)
		{
			qWarning () << "no 'from' data";
		}

		try
		{
			auto origDate = header->Date ()->getValue ().dynamicCast<const vmime::datetime> ();
			auto date = vmime::utility::datetimeUtils::toUniversalTime (*origDate);
			QDate qdate (date.getYear (), date.getMonth (), date.getDay ());
			QTime time (date.getHour (), date.getMinute (), date.getSecond ());
			msg->SetDate (QDateTime (qdate, time, Qt::UTC));
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		try
		{
			auto str = header->Subject ()->getValue ()
					.dynamicCast<const vmime::text> ()->getConvertedText (utf8cs);
			msg->SetSubject (QString::fromUtf8 (str.c_str ()));
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		return msg;
	}

	namespace
	{
		vmime::ref<vmime::message> FromNetMessage (vmime::ref<vmime::net::message> msg)
		{
			return msg->getParsedMessage ();
		}
	}

	void AccountThreadWorker::FetchMessagesPOP3 (Account::FetchFlags fetchFlags)
	{
		auto store = MakeStore ();
		store->setProperty ("connection.tls", true);
		store->connect ();
		auto folder = store->getDefaultFolder ();
		folder->open (vmime::net::folder::MODE_READ_WRITE);

		auto messages = folder->getMessages ();
		if (!messages.size ())
			return;

		qDebug () << "know about" << messages.size () << "messages";
		int desiredFlags = vmime::net::folder::FETCH_FLAGS |
					vmime::net::folder::FETCH_SIZE |
					vmime::net::folder::FETCH_UID |
					vmime::net::folder::FETCH_ENVELOPE;
		desiredFlags &= folder->getFetchCapabilities ();

		qDebug () << "folder supports" << folder->getFetchCapabilities ()
				<< "so we gonna fetch" << desiredFlags;

		try
		{
			auto context = tr ("Fetching headers for %1")
					.arg (A_->GetName ());

			auto pl = new ProgressListener (context);
			pl->deleteLater ();
			emit gotProgressListener (ProgressListener_g_ptr (pl));

			folder->fetchMessages (messages, desiredFlags, pl);
		}
		catch (const vmime::exceptions::operation_not_supported& ons)
		{
			qWarning () << Q_FUNC_INFO
					<< "fetch operation not supported:"
					<< ons.what ();
			return;
		}

		if (fetchFlags & Account::FetchNew)
		{
			if (folder->getFetchCapabilities () & vmime::net::folder::FETCH_FLAGS)
			{
				auto pos = std::remove_if (messages.begin (), messages.end (),
						[] (decltype (messages.front ()) msg) { return msg->getFlags () & vmime::net::message::FLAG_SEEN; });
				messages.erase (pos, messages.end ());
				qDebug () << "fetching only new msgs:" << messages.size ();
			}
			else
			{
				qDebug () << "folder hasn't advertised support for flags :(";
			}
		}

		auto newMessages = FetchFullMessages (messages);

		emit gotMsgHeaders (newMessages);
	}

	void AccountThreadWorker::FetchMessagesIMAP (Account::FetchFlags fetchFlags)
	{
		auto store = MakeStore ();
		store->connect ();
		auto folder = store->getDefaultFolder ();
		folder->open (vmime::net::folder::MODE_READ_WRITE);

		auto messages = folder->getMessages ();
		if (!messages.size ())
			return;

		const int desiredFlags = vmime::net::folder::FETCH_FLAGS |
					vmime::net::folder::FETCH_SIZE |
					vmime::net::folder::FETCH_UID |
					vmime::net::folder::FETCH_ENVELOPE;

		try
		{
			const auto& context = tr ("Fetching headers for %1")
					.arg (A_->GetName ());

			auto pl = new ProgressListener (context);
			pl->deleteLater ();
			emit gotProgressListener (ProgressListener_g_ptr (pl));

			folder->fetchMessages (messages, desiredFlags, pl);
		}
		catch (const vmime::exceptions::operation_not_supported& ons)
		{
			qWarning () << Q_FUNC_INFO
					<< "fetch operation not supported:"
					<< ons.what ();
			return;
		}

		const QSet<QByteArray>& existing = Core::Instance ().GetStorage ()->LoadIDs (A_);

		QList<Message_ptr> newMessages;
		std::transform (messages.begin (), messages.end (), std::back_inserter (newMessages),
				[this] (decltype (messages.front ()) msg) { return FromHeaders (msg); });

		QList<Message_ptr> updatedMessages;
		Q_FOREACH (Message_ptr msg, newMessages)
		{
			if (!existing.contains (msg->GetID ()))
				continue;

			newMessages.removeAll (msg);

			auto updated = Core::Instance ().GetStorage ()->
					LoadMessage (A_, msg->GetID ());
			updated->SetRead (msg->IsRead ());
			updatedMessages << updated;
		}

		emit gotMsgHeaders (newMessages);
		emit gotUpdatedMessages (updatedMessages);
	}

	namespace
	{
		template<typename T>
		QString Stringize (const T& t, const vmime::charset& source)
		{
			vmime::string str;
			vmime::utility::outputStreamStringAdapter out (str);
			vmime::utility::charsetFilteredOutputStream fout (source,
					vmime::charset ("utf-8"), out);

			t->extract (fout);
			fout.flush ();

			return QString::fromUtf8 (str.c_str ());
		}

		template<typename T>
		QString Stringize (const T& t)
		{
			vmime::string str;
			vmime::utility::outputStreamStringAdapter out (str);

			t->extract (out);

			return QString::fromUtf8 (str.c_str ());
		}

		void FullifyHeaderMessage (Message_ptr msg, const vmime::ref<vmime::message>& full)
		{
			vmime::messageParser mp (full);

			QString html;
			QString plain;

			Q_FOREACH (auto tp, mp.getTextPartList ())
			{
				if (tp->getType ().getType () != vmime::mediaTypes::TEXT)
				{
					qWarning () << Q_FUNC_INFO
							<< "non-text in text part"
							<< tp->getType ().getType ().c_str ();
					continue;
				}

				if (tp->getType ().getSubType () == vmime::mediaTypes::TEXT_HTML)
				{
					auto htp = tp.dynamicCast<const vmime::htmlTextPart> ();
					html = Stringize (htp->getText (), htp->getCharset ());
					plain = Stringize (htp->getPlainText (), htp->getCharset ());
				}
				else if (plain.isEmpty () &&
						tp->getType ().getSubType () == vmime::mediaTypes::TEXT_PLAIN)
					plain = Stringize (tp->getText (), tp->getCharset ());
			}

			msg->SetBody (plain);
			msg->SetHTMLBody (html);
		}
	}

	QList<Message_ptr> AccountThreadWorker::FetchFullMessages (const std::vector<vmime::utility::ref<vmime::net::message>>& messages)
	{
		const auto& context = tr ("Fetching messages for %1")
					.arg (A_->GetName ());

		auto pl = new ProgressListener (context);
		pl->deleteLater ();
		emit gotProgressListener (ProgressListener_g_ptr (pl));

		QMetaObject::invokeMethod (pl,
				"start",
				Q_ARG (const int, messages.size ()));

		int i = 0;
		QList<Message_ptr> newMessages;
		Q_FOREACH (auto message, messages)
		{
			QMetaObject::invokeMethod (pl,
					"progress",
					Q_ARG (const int, ++i),
					Q_ARG (const int, messages.size ()));

			auto msgObj = FromHeaders (message);

			FullifyHeaderMessage (msgObj, FromNetMessage (message));

			newMessages << msgObj;
		}

		QMetaObject::invokeMethod (pl,
					"stop",
					Q_ARG (const int, messages.size ()));

		return newMessages;
	}

	void AccountThreadWorker::fetchNewHeaders (Account::FetchFlags flags)
	{
		switch (A_->InType_)
		{
		case Account::InType::POP3:
			FetchMessagesPOP3 (flags);
			break;
		case Account::InType::IMAP:
			FetchMessagesIMAP (flags);
			break;
		case Account::InType::Maildir:
			break;
		}
	}

	void AccountThreadWorker::fetchWholeMessage (Message_ptr origMsg)
	{
		if (!origMsg)
			return;

		if (A_->InType_ == Account::InType::POP3)
			return;

		const QByteArray& sid = origMsg->GetID ();
		vmime::string id (sid.constData ());
		qDebug () << Q_FUNC_INFO << sid.toHex ();

		auto store = MakeStore ();
		store->connect ();

		auto folder = store->getDefaultFolder ();
		folder->open (vmime::net::folder::MODE_READ_WRITE);

		auto messages = folder->getMessages ();
		folder->fetchMessages (messages, vmime::net::folder::FETCH_UID);

		auto pos = std::find_if (messages.begin (), messages.end (),
				[id] (const vmime::ref<vmime::net::message>& message) { return message->getUniqueId () == id; });
		if (pos == messages.end ())
		{
			Q_FOREACH (auto msg, messages)
				qWarning () << QByteArray (msg->getUniqueId ().c_str ()).toHex ();
			qWarning () << Q_FUNC_INFO
					<< "message with ID"
					<< sid.toHex ()
					<< "not found in"
					<< messages.size ();
			return;
		}

		qDebug () << "found corresponding message, fullifying...";

		folder->fetchMessage (*pos, vmime::net::folder::FETCH_FLAGS |
					vmime::net::folder::FETCH_UID |
					vmime::net::folder::FETCH_CONTENT_INFO |
					vmime::net::folder::FETCH_STRUCTURE |
					vmime::net::folder::FETCH_FULL_HEADER);

		FullifyHeaderMessage (origMsg, FromNetMessage (*pos));

		qDebug () << "done";

		emit messageBodyFetched (origMsg);
	}
}
}
