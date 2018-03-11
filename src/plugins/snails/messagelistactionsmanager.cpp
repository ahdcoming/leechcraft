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

#include "messagelistactionsmanager.h"
#include <QUrl>
#include <QMessageBox>
#include <QTextDocument>
#include <QtDebug>
#include <vmime/messageIdSequence.hpp>
#include <util/xpc/util.h>
#include <util/threads/futures.h>
#include <util/sll/urlaccessor.h>
#include <util/sll/either.h>
#include <util/sll/visitor.h>
#include <interfaces/core/ientitymanager.h>
#include "core.h"
#include "message.h"
#include "vmimeconversions.h"
#include "account.h"
#include "storage.h"
#include "accountdatabase.h"

namespace LeechCraft
{
namespace Snails
{
	using Header_ptr = vmime::shared_ptr<const vmime::header>;

	class MessageListActionsProvider
	{
	public:
		virtual QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr&, Account*) const = 0;
	};

	namespace
	{
		vmime::shared_ptr<const vmime::messageId> GetGithubMsgId (const Header_ptr& headers)
		{
			const auto& referencesField = headers->References ();
			if (!referencesField)
			{
				if (const auto msgId = headers->MessageId ())
					return msgId->getValue<vmime::messageId> ();
				return {};
			}

			const auto& refSeq = referencesField->getValue<vmime::messageIdSequence> ();
			if (!refSeq)
				return {};

			if (!refSeq->getMessageIdCount ())
				return {};

			return refSeq->getMessageIdAt (0);
		}

		QString GetGithubAddr (const Header_ptr& headers)
		{
			if (!headers->findField ("X-GitHub-Sender"))
				return {};

			const auto& ref = GetGithubMsgId (headers);
			if (!ref)
				return {};

			return QString::fromUtf8 (ref->getLeft ().c_str ());
		}

		class GithubProvider final : public MessageListActionsProvider
		{
		public:
			QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr& headers, Account*) const override
			{
				const auto& addrReq = GetGithubAddr (headers);
				if (addrReq.isEmpty ())
					return {};

				return
				{
					{
						QObject::tr ("Open"),
						QObject::tr ("Open the page on GitHub."),
						QIcon::fromTheme ("document-open"),
						[addrReq] (const Message_ptr&)
						{
							const QUrl fullUrl { "https://github.com/" + addrReq };
							const auto& entity = Util::MakeEntity (fullUrl, {}, FromUserInitiated | OnlyHandle);
							Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
						},
						{}
					}
				};
			}
		};

		class BugzillaProvider final : public MessageListActionsProvider
		{
		public:
			QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr& headers, Account*) const override
			{
				const auto header = headers->findField ("X-Bugzilla-URL");
				if (!header)
					return {};

				const auto& urlText = header->getValue<vmime::text> ();
				if (!urlText)
					return {};

				const auto& url = StringizeCT (*urlText);

				const auto& referencesField = headers->MessageId ();
				if (!referencesField)
					return {};

				const auto& ref = referencesField->getValue<vmime::messageId> ();
				if (!ref)
					return {};

				const auto& left = QString::fromUtf8 (ref->getLeft ().c_str ());
				const auto bugId = left.section ('-', 1, 1);

				return
				{
					{
						QObject::tr ("Open"),
						QObject::tr ("Open the bug page on Bugzilla."),
						QIcon::fromTheme ("tools-report-bug"),
						[url, bugId] (const Message_ptr&)
						{
							const QUrl fullUrl { url + "show_bug.cgi?id=" + bugId };
							const auto& entity = Util::MakeEntity (fullUrl, {}, FromUserInitiated | OnlyHandle);
							Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
						},
						{}
					}
				};
			}
		};

		class RedmineProvider final : public MessageListActionsProvider
		{
		public:
			QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr& headers, Account*) const override
			{
				const auto header = headers->findField ("X-Redmine-Host");
				if (!header)
					return {};

				const auto& urlText = header->getValue<vmime::text> ();
				if (!urlText)
					return {};

				const auto& url = StringizeCT (*urlText);

				const auto issueHeader = headers->findField ("X-Redmine-Issue-Id");
				if (!issueHeader)
					return {};

				const auto& issueText = issueHeader->getValue<vmime::text> ();
				if (!issueText)
					return {};

				const auto& issue = StringizeCT (*issueText);

				return
				{
					{
						QObject::tr ("Open"),
						QObject::tr ("Open the issue page on Redmine."),
						QIcon::fromTheme ("tools-report-bug"),
						[url, issue] (const Message_ptr&)
						{
							const QUrl fullUrl { "http://" + url + "/issues/" + issue };
							const auto& entity = Util::MakeEntity (fullUrl, {}, FromUserInitiated | OnlyHandle);
							Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
						},
						{}
					}
				};
			}
		};

		class ReviewboardProvider final : public MessageListActionsProvider
		{
		public:
			QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr& headers, Account*) const override
			{
				const auto header = headers->findField ("X-ReviewRequest-URL");
				if (!header)
					return {};

				const auto& urlText = header->getValue<vmime::text> ();
				if (!urlText)
					return {};

				const auto& url = StringizeCT (*urlText);

				return
				{
					{
						QObject::tr ("Open"),
						QObject::tr ("Open the review page on ReviewBoard."),
						QIcon::fromTheme ("document-open"),
						[url] (const Message_ptr&)
						{
							const auto& entity = Util::MakeEntity (QUrl { url }, {}, FromUserInitiated | OnlyHandle);
							Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
						},
						{}
					}
				};
			}
		};

		QUrl GetUnsubscribeUrl (const QString& text)
		{
			const auto& parts = text.split (',', QString::SkipEmptyParts);

			QUrl email;
			QUrl url;
			for (auto part : parts)
			{
				part = part.simplified ();
				if (part.startsWith ('<'))
					part = part.mid (1, part.size () - 2);

				const auto& ascii = part.toLatin1 ();

				if (ascii.startsWith ("mailto:"))
				{
					const auto& testEmail = QUrl::fromEncoded (ascii);
					if (testEmail.isValid ())
						email = testEmail;
				}
				else
				{
					const auto& testUrl = QUrl::fromEncoded (ascii);
					if (testUrl.isValid ())
						url = testUrl;
				}
			}

			return email.isValid () ? email : url;
		}

		QString GetListName (const Message_ptr& msg, const Header_ptr& headers)
		{
			const auto& addrString = "<em>" + msg->GetAddressString (Message::Address::From).toHtmlEscaped () + "</em>";

			const auto header = headers->findField ("List-Id");
			if (!header)
				return addrString;

			const auto& vmimeText = header->getValue<vmime::text> ();
			if (!vmimeText)
				return addrString;

			return "<em>" + StringizeCT (*vmimeText).toHtmlEscaped () + "</em>";
		}

		void HandleUnsubscribeText (const QString& text, const Message_ptr& msg, const Header_ptr& headers, Account *acc)
		{
			const auto& url = GetUnsubscribeUrl (text);

			const auto& addrString = GetListName (msg, headers);
			if (url.scheme () == "mailto")
			{
				if (QMessageBox::question (nullptr,
						QObject::tr ("Unsubscription confirmation"),
						QObject::tr ("Are you sure you want to unsubscribe from %1? "
							"This will send an email to %2.")
							.arg (addrString)
							.arg ("<em>" + url.path ().toHtmlEscaped () + "</em>"),
						QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
					return;

				const auto& msg = std::make_shared<Message> ();
				msg->SetAddress (Message::Address::To, { {}, url.path () });

				const auto& subjQuery = Util::UrlAccessor { url } ["subject"];
				msg->SetSubject (subjQuery.isEmpty () ? "unsubscribe" : subjQuery);

				Util::Sequence (nullptr, acc->SendMessage (msg)) >>
						[url] (const auto& result)
						{
							const auto& entity = Util::Visit (result.AsVariant (),
									[url] (Util::Void)
									{
										return Util::MakeNotification ("Snails",
												QObject::tr ("Successfully sent unsubscribe request to %1.")
													.arg ("<em>" + url.path () + "</em>"),
												PInfo_);
									},
									[url] (const auto& err)
									{
										const auto& msg = Util::Visit (err,
												[] (const auto& err) { return QString::fromUtf8 (err.what ()); });
										return Util::MakeNotification ("Snails",
												QObject::tr ("Unable to send unsubscribe request to %1: %2.")
													.arg ("<em>" + url.path () + "</em>")
													.arg (msg),
												PWarning_);
									});
							Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
						};
			}
			else
			{
				if (QMessageBox::question (nullptr,
						QObject::tr ("Unsubscription confirmation"),
						QObject::tr ("Are you sure you want to unsubscribe from %1? "
							"This will open the following web page in your browser: %2")
							.arg (addrString)
							.arg ("<br/><em>" + url.toString () + "</em>"),
						QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
					return;

				const auto& entity = Util::MakeEntity (url, {}, FromUserInitiated | OnlyHandle);
				Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (entity);
			}
		}

		class UnsubscribeProvider final : public MessageListActionsProvider
		{
		public:
			QList<MessageListActionInfo> GetMessageActions (const Message_ptr&, const Header_ptr& headers, Account *acc) const override
			{
				const auto header = headers->findField ("List-Unsubscribe");
				if (!header)
					return {};

				return
				{
					{
						QObject::tr ("Unsubscribe"),
						QObject::tr ("Try cancelling receiving further messages like this."),
						QIcon::fromTheme ("news-unsubscribe"),
						[header, headers, acc] (const Message_ptr& msg)
						{
							const auto& vmimeText = header->getValue<vmime::text> ();
							if (!vmimeText)
								return;

							HandleUnsubscribeText (StringizeCT (*vmimeText), msg, headers, acc);
						},
						{}
					}
				};
			}
		};

		class AttachmentsProvider final : public MessageListActionsProvider
		{
			Account * const Acc_;
		public:
			AttachmentsProvider (Account *acc)
			: Acc_ { acc }
			{
			}

			QList<MessageListActionInfo> GetMessageActions (const Message_ptr& msg, const Header_ptr& headers, Account *acc) const override
			{
				if (msg->GetAttachments ().isEmpty ())
					return {};

				return
				{
					{
						QObject::tr ("Attachments"),
						QObject::tr ("Open/save attachments."),
						QIcon::fromTheme ("mail-attachment"),
						[acc = Acc_] (const Message_ptr& msg)
						{
							//new MessageAttachmentsDialog { acc, msg };
						},
						{}
					}
				};
			}
		};
	}

	MessageListActionsManager::MessageListActionsManager (Account *acc, QObject *parent)
	: QObject { parent }
	, Acc_ { acc }
	{
		Providers_ << std::make_shared<AttachmentsProvider> (acc);
		Providers_ << std::make_shared<GithubProvider> ();
		Providers_ << std::make_shared<RedmineProvider> ();
		Providers_ << std::make_shared<BugzillaProvider> ();
		Providers_ << std::make_shared<ReviewboardProvider> ();
		Providers_ << std::make_shared<UnsubscribeProvider> ();
	}

	QList<MessageListActionInfo> MessageListActionsManager::GetMessageActions (const Message_ptr& msg) const
	{
		const auto headerText = Acc_->GetDatabase ()->GetMessageHeader (msg->GetMessageID ());
		if (!headerText)
			return {};

		auto header = vmime::make_shared<vmime::header> ();
		header->parse ({ headerText->constData (), static_cast<size_t> (headerText->size ()) });

		QList<MessageListActionInfo> result;
		for (const auto provider : Providers_)
			result += provider->GetMessageActions (msg, header, Acc_);
		return result;
	}
}
}
