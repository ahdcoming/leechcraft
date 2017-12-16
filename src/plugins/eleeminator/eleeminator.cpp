/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2014  Georg Rudoy
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

#include "eleeminator.h"
#include <QIcon>
#include <QtDebug>
#include <util/util.h>
#include <util/shortcuts/shortcutmanager.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <interfaces/core/iiconthememanager.h>
#include "termtab.h"
#include "xmlsettingsmanager.h"
#include "colorschemesmanager.h"

namespace LeechCraft
{
namespace Eleeminator
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		ShortcutMgr_ = new Util::ShortcutManager { proxy };
		ShortcutMgr_->SetObject (this);

		ShortcutMgr_->RegisterActionInfo (GetUniqueID () + ".Close",
				{
					tr ("Close terminal tab"),
					QString { "Ctrl+Shift+W" },
					proxy->GetIconThemeManager ()->GetIcon ("tab-close")
				});
		ShortcutMgr_->RegisterActionInfo (GetUniqueID () + ".Clear",
				{
					tr ("Clear terminal window"),
					QString { "Ctrl+Shift+L" },
					proxy->GetIconThemeManager ()->GetIcon ("edit-clear")
				});
		ShortcutMgr_->RegisterActionInfo (GetUniqueID () + ".Copy",
				{
					tr ("Copy selected text to clipboard"),
					QString { "Ctrl+Shift+C" },
					proxy->GetIconThemeManager ()->GetIcon ("edit-copy")
				});
		ShortcutMgr_->RegisterActionInfo (GetUniqueID () + ".Paste",
				{
					tr ("Paste text from clipboard"),
					QString { "Ctrl+Shift+V" },
					proxy->GetIconThemeManager ()->GetIcon ("edit-paste")
				});

		ColorSchemesMgr_ = new ColorSchemesManager;

		Util::InstallTranslator ("eleeminator");

		TermTabTC_ =
		{
			GetUniqueID () + ".TermTab",
			tr ("Terminal"),
			tr ("Termianl emulator."),
			GetIcon (),
			15,
			TFOpenableByRequest | TFOverridesTabClose
		};

		XSD_ = std::make_shared<Util::XmlSettingsDialog> ();
		XSD_->RegisterObject (&XmlSettingsManager::Instance (), "eleeminatorsettings.xml");
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Eleeminator";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Eleeminator";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Embedded LeechCraft terminal emulator.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon { "lcicons:/resources/images/eleeminator.svg" };
		return icon;
	}

	TabClasses_t Plugin::GetTabClasses () const
	{
		return { TermTabTC_ };
	}

	void Plugin::TabOpenRequested (const QByteArray& tc)
	{
		if (tc == TermTabTC_.TabClass_)
		{
			auto tab = new TermTab { Proxy_, ShortcutMgr_, TermTabTC_, ColorSchemesMgr_, this };
			emit addNewTab (TermTabTC_.VisibleName_, tab);
			emit raiseTab (tab);

			connect (tab,
					SIGNAL (changeTabName (QWidget*, QString)),
					this,
					SIGNAL (changeTabName (QWidget*, QString)));
			connect (tab,
					SIGNAL (remove (QWidget*)),
					this,
					SIGNAL (removeTab (QWidget*)));
		}
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tc;
	}

	QMap<QString, ActionInfo> Plugin::GetActionInfo () const
	{
		return ShortcutMgr_->GetActionInfo ();
	}

	void Plugin::SetShortcut (const QString& id, const QKeySequences_t& sequences)
	{
		ShortcutMgr_->SetShortcut (id, sequences);
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XSD_;
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_eleeminator, LeechCraft::Eleeminator::Plugin);
