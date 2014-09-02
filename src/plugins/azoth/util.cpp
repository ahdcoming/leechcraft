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

#include "util.h"
#include <cmath>
#include <QString>
#include <QWizard>
#include <QList>
#include <util/xpc/util.h>
#include <interfaces/an/constants.h>
#include <interfaces/structures.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/irootwindowsmanager.h>
#include "interfaces/azoth/iclentry.h"
#include "interfaces/azoth/iaccount.h"
#include "addaccountwizardfirstpage.h"
#include "core.h"
#include "chattabsmanager.h"
#include "xmlsettingsmanager.h"

Q_DECLARE_METATYPE (QList<QColor>);

namespace LeechCraft
{
namespace Azoth
{
	void BuildNotification (Entity& e, ICLEntry *other)
	{
		e.Additional_ ["NotificationPixmap"] =
				QVariant::fromValue<QPixmap> (QPixmap::fromImage (other->GetAvatar ()));
		e.Additional_ ["org.LC.AdvNotifications.SenderID"] = "org.LeechCraft.Azoth";
		e.Additional_ ["org.LC.AdvNotifications.EventCategory"] = AN::CatIM;
		e.Additional_ ["org.LC.AdvNotifications.EventID"] =
				"org.LC.Plugins.Azoth.IncomingMessageFrom/" + other->GetEntryID ();

		e.Additional_ ["org.LC.AdvNotifications.VisualPath"] = QStringList (other->GetEntryName ());

		int win = 0;
		const auto tab = Core::Instance ()
				.GetChatTabsManager ()->GetChatTab (other->GetEntryID ());

		const auto rootWM = Core::Instance ().GetProxy ()->GetRootWindowsManager ();
		if (tab)
			win = rootWM->GetWindowForTab (tab);
		if (!tab || win == -1)
		{
			const auto& tc = other->GetEntryType () == ICLEntry::EntryType::MUC ?
					ChatTab::GetMUCTabClassInfo () :
					ChatTab::GetChatTabClassInfo ();
			win = rootWM->GetPreferredWindowIndex (tc.TabClass_);
		}

		e.Additional_ ["org.LC.AdvNotifications.WindowIndex"] = win;

		e.Additional_ ["org.LC.Plugins.Azoth.SourceName"] = other->GetEntryName ();
		e.Additional_ ["org.LC.Plugins.Azoth.SourceID"] = other->GetEntryID ();
		e.Additional_ ["org.LC.Plugins.Azoth.SourceGroups"] = other->Groups ();
	}

	QString GetActivityIconName (const QString& general, const QString& specific)
	{
		return (general + ' ' + specific).trimmed ().replace (' ', '_');
	}

	void InitiateAccountAddition(QWidget *parent)
	{
		QWizard *wizard = new QWizard (parent);
		wizard->setAttribute (Qt::WA_DeleteOnClose);
		wizard->setWindowTitle (QObject::tr ("Add account"));
		wizard->addPage (new AddAccountWizardFirstPage (wizard));

		wizard->show ();
	}

	void AuthorizeEntry (ICLEntry *entry)
	{
		IAccount *account =
				qobject_cast<IAccount*> (entry->GetParentAccount ());
		if (!account)
		{
			qWarning () << Q_FUNC_INFO
					<< "parent account doesn't implement IAccount:"
					<< entry->GetParentAccount ();
			return;
		}
		const QString& id = entry->GetHumanReadableID ();
		account->Authorize (entry->GetQObject ());
		account->RequestAuth (id);

		const auto& e = Util::MakeANCancel ("org.LeechCraft.Azoth",
				"org.LC.Plugins.Azoth.AuthRequestFrom/" + entry->GetEntryID ());
		Core::Instance ().SendEntity (e);
	}

	void DenyAuthForEntry (ICLEntry *entry)
	{
		IAccount *account =
				qobject_cast<IAccount*> (entry->GetParentAccount ());
		if (!account)
		{
			qWarning () << Q_FUNC_INFO
					<< "parent account doesn't implement IAccount:"
					<< entry->GetParentAccount ();
			return;
		}
		account->DenyAuth (entry->GetQObject ());

		const auto& e = Util::MakeANCancel ("org.LeechCraft.Azoth",
				"org.LC.Plugins.Azoth.AuthRequestFrom/" + entry->GetEntryID ());
		Core::Instance ().SendEntity (e);
	}

	QObject* FindByHRId (IAccount *acc, const QString& hrId)
	{
		const auto& allEntries = acc->GetCLEntries ();
		const auto pos = std::find_if (allEntries.begin (), allEntries.end (),
				[&hrId] (QObject *obj)
				{
					return qobject_cast<ICLEntry*> (obj)->GetHumanReadableID () == hrId;
				});

		return pos == allEntries.end () ? nullptr : *pos;
	}

	QList<QColor> GenerateColors (const QString& coloring, QColor bg)
	{
		auto compatibleColors = [] (const QColor& c1, const QColor& c2) -> bool
		{
			int dR = c1.red () - c2.red ();
			int dG = c1.green () - c2.green ();
			int dB = c1.blue () - c2.blue ();

			double dV = std::abs (c1.value () - c2.value ());
			double dC = std::sqrt (0.2126 * dR * dR + 0.7152 * dG * dG + 0.0722 * dB * dB);

			if ((dC < 80. && dV > 100.) ||
					(dC < 110. && dV <= 100. && dV > 10.) ||
					(dC < 125. && dV <= 10.))
				return false;

			return true;
		};

		QList<QColor> result;
		if (XmlSettingsManager::Instance ().property ("OverrideHashColors").toBool ())
		{
			result = XmlSettingsManager::Instance ()
					.property ("OverrideColorsList").value<decltype (result)> ();
			if (!result.isEmpty ())
				return result;
		}

		if (coloring == "hash" || coloring.isEmpty ())
		{
			if (!bg.isValid ())
				bg = QApplication::palette ().color (QPalette::Base);

			int alpha = bg.alpha ();

			QColor color;
			for (int hue = 0; hue < 360; hue += 18)
			{
				color.setHsv (hue, 255, 255, alpha);
				if (compatibleColors (color, bg))
					result << color;
				color.setHsv (hue, 255, 170, alpha);
				if (compatibleColors (color, bg))
					result << color;
			}
		}
		else
			for (const auto& str : coloring.split (' ', QString::SkipEmptyParts))
				result << QColor (str);

		return result;
	}

	QString GetNickColor (const QString& nick, const QList<QColor>& colors)
	{
		if (colors.isEmpty ())
			return "green";

		int hash = 0;
		for (int i = 0; i < nick.length (); ++i)
		{
			const QChar& c = nick.at (i);
			hash += c.toLatin1 () ?
					c.toLatin1 () :
					c.unicode ();
			hash += nick.length ();
		}
		hash = std::abs (hash);
		const auto& nc = colors.at (hash % colors.size ());
		return nc.name ();
	}

	QStringList GetMucParticipants (const QString& entryId)
	{
		const auto entry = qobject_cast<IMUCEntry*> (Core::Instance ().GetEntry (entryId));
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< entry
					<< "doesn't implement IMUCEntry";
			return {};
		}

		QStringList participantsList;
		for (const auto item : entry->GetParticipants ())
		{
			const auto part = qobject_cast<ICLEntry*> (item);
			if (!part)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to cast item to ICLEntry"
						<< item;
				continue;
			}
			participantsList << part->GetEntryName ();
		}
		return participantsList;
	}
}
}
