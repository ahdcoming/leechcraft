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

#include "pierre.h"
#include <QIcon>
#include <QMenuBar>
#include <QMainWindow>
#include <QTimer>
#include <QSystemTrayIcon>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/entitytesthandleresult.h>
#include <interfaces/imwproxy.h>
#include <interfaces/iactionsexporter.h>
#include "fullscreen.h"

extern void qt_mac_set_dock_menu (QMenu*);

namespace LeechCraft
{
namespace Pierre
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		TrayIconMenu_ = 0;

		Proxy_ = proxy;
		MenuBar_ = new QMenuBar (0);
	}

	void Plugin::SecondInit ()
	{
		auto rootWM = Proxy_->GetRootWindowsManager ();
		for (int i = 0; i < rootWM->GetWindowsCount (); ++i)
			handleWindow (i);

		connect (rootWM->GetQObject (),
				SIGNAL (windowAdded (int)),
				this,
				SLOT (handleWindow (int)));
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Pierre";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Pierre";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Pierre d'Olle is the Mac OS X integration layer.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Core.Plugins/1.0";
		return result;
	}

	void Plugin::hookGonnaFillMenu (IHookProxy_ptr)
	{
		QTimer::singleShot (0,
				this,
				SLOT (fillMenu ()));
	}

	void Plugin::hookTrayIconCreated (IHookProxy_ptr proxy, QSystemTrayIcon *icon)
	{
		TrayIconMenu_ = icon->contextMenu ();
		qt_mac_set_dock_menu (TrayIconMenu_);
	}

	void Plugin::hookTrayIconVisibilityChanged (IHookProxy_ptr proxy, QSystemTrayIcon*, bool)
	{
		proxy->CancelDefault ();
	}

	void Plugin::handleGotActions (const QList<QAction*>&, ActionsEmbedPlace aep)
	{
		if (aep != ActionsEmbedPlace::ToolsMenu)
			return;

		MenuBar_->clear ();
		QTimer::singleShot (0,
				this,
				SLOT (fillMenu ()));
	}

	void Plugin::handleWindow (int index)
	{
		qDebug () << Q_FUNC_INFO;
		auto rootWM = Proxy_->GetRootWindowsManager ();
		FS::AddAction (rootWM->GetMainWindow (index));
	}

	void Plugin::fillMenu ()
	{
		auto rootWM = Proxy_->GetRootWindowsManager ();
		auto menu = rootWM->GetMWProxy (0)->GetMainMenu ();

		QMenu *lcMenu = 0;
		QList<QAction*> firstLevelActions;
		for (auto action : menu->actions ())
			if (action->menu ())
			{
				MenuBar_->addAction (action);
				if (!lcMenu)
					lcMenu = action->menu ();
			}
			else
			{
				if (action->menuRole () == QAction::TextHeuristicRole)
					action->setMenuRole (QAction::ApplicationSpecificRole);
				firstLevelActions << action;
			}

		for (auto act : firstLevelActions)
			lcMenu->addAction (act);

		if (!lcMenu->actions ().isEmpty ())
			MenuBar_->addMenu (lcMenu);

		const auto& actors = Proxy_->GetPluginsManager ()->
				GetAllCastableRoots<IActionsExporter*> ();
		for (auto actor : actors)
			connect (actor,
					SIGNAL (gotActions (QList<QAction*>, LeechCraft::ActionsEmbedPlace)),
					this,
					SLOT (handleGotActions (QList<QAction*>, LeechCraft::ActionsEmbedPlace)),
					Qt::UniqueConnection);
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_pierre, LeechCraft::Pierre::Plugin);
