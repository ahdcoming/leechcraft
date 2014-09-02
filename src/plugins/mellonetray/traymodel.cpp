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

#include "traymodel.h"
#include <QAbstractEventDispatcher>
#include <QtDebug>
#include <util/x11/xwrapper.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xdamage.h>

#if QT_VERSION >= 0x050000
#include <xcb/xcb.h>
#include <xcb/damage.h>
#endif

namespace LeechCraft
{
namespace Mellonetray
{
	namespace
	{
		VisualID GetVisual ()
		{
			VisualID result = 0;
			auto disp = Util::XWrapper::Instance ().GetDisplay ();

			XVisualInfo init;
			init.screen = QX11Info::appScreen ();
			init.depth = 32;
			init.c_class = TrueColor;

			int nvi = 0;
			auto xvi = XGetVisualInfo (disp, VisualScreenMask | VisualDepthMask | VisualClassMask, &init, &nvi);
			if (!xvi)
				return result;

			for (int i = 0; i < nvi; ++i)
			{
				auto fmt = XRenderFindVisualFormat (disp, xvi [i].visual);
				if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask)
				{
					result = xvi [i].visualid;
					break;
				}
			}

			XFree (xvi);

			return result;
		}

#if QT_VERSION < 0x050000
		bool EvFilter (void *event)
		{
			return TrayModel::Instance ().Filter (static_cast<XEvent*> (event));
		}
#endif
	}

	TrayModel::TrayModel ()
#if QT_VERSION < 0x050000
	: PrevFilter_ (QAbstractEventDispatcher::instance ()->setEventFilter (EvFilter))
#endif
	{
#if QT_VERSION >= 0x050000
		QAbstractEventDispatcher::instance ()->installNativeEventFilter (this);
#endif

		QHash<int, QByteArray> roleNames;
		roleNames [Role::ItemID] = "itemID";
		setRoleNames (roleNames);

		auto& w = Util::XWrapper::Instance ();
		const auto disp = w.GetDisplay ();
		const auto rootWin = w.GetRootWindow ();

		const auto atom = w.GetAtom (QString ("_NET_SYSTEM_TRAY_S%1").arg (DefaultScreen (disp)));

		if (XGetSelectionOwner (disp, atom) != None)
		{
			qWarning () << Q_FUNC_INFO
					<< "another system tray is active";
			return;
		}

		TrayWinID_ = XCreateSimpleWindow (disp, rootWin, -1, -1, 1, 1, 0, 0, 0);
		XSetSelectionOwner (disp, atom, TrayWinID_, CurrentTime);
		if (XGetSelectionOwner (disp, atom) != TrayWinID_)
		{
			qWarning () << Q_FUNC_INFO
					<< "call to XSetSelectionOwner failed";
			return;
		}

		int orientation = 0;
		XChangeProperty (disp,
				TrayWinID_,
				w.GetAtom ("_NET_SYSTEM_TRAY_ORIENTATION"),
				XA_CARDINAL,
				32,
				PropModeReplace,
				reinterpret_cast<uchar*> (&orientation),
				1);

		if (auto visual = GetVisual ())
			XChangeProperty (disp,
					TrayWinID_,
					w.GetAtom ("_NET_SYSTEM_TRAY_VISUAL"),
					XA_VISUALID,
					32,
					PropModeReplace,
					reinterpret_cast<uchar*> (&visual),
					1);

		XClientMessageEvent ev;
		ev.type = ClientMessage;
		ev.window = rootWin;
		ev.message_type = w.GetAtom ("MANAGER");
		ev.format = 32;
		ev.data.l [0] = CurrentTime;
		ev.data.l [1] = atom;
		ev.data.l [2] = TrayWinID_;
		ev.data.l [3] = 0;
		ev.data.l [4] = 0;
		XSendEvent (disp, rootWin, False, StructureNotifyMask, reinterpret_cast<XEvent*> (&ev));

		int damageErr = 0;
		XDamageQueryExtension (disp, &DamageEvent_, &damageErr);

		IsValid_ = true;
	}

	TrayModel& TrayModel::Instance ()
	{
		static TrayModel m;
		return m;
	}

	void TrayModel::Release ()
	{
		if (TrayWinID_)
			XDestroyWindow (Util::XWrapper::Instance ().GetDisplay (), TrayWinID_);
	}

	bool TrayModel::IsValid () const
	{
		return IsValid_;
	}

	int TrayModel::columnCount (const QModelIndex&) const
	{
		return 1;
	}

	int TrayModel::rowCount (const QModelIndex& parent) const
	{
		return parent.isValid () ? 0 : Items_.size ();
	}

	QModelIndex TrayModel::index (int row, int column, const QModelIndex& parent) const
	{
		return hasIndex (row, column, parent) ?
				createIndex (row, column) :
				QModelIndex {};
	}

	QModelIndex TrayModel::parent (const QModelIndex&) const
	{
		return {};
	}

	QVariant TrayModel::data (const QModelIndex& index, int role) const
	{
		const auto row = index.row ();
		const auto& item = Items_.at (row);

		switch (role)
		{
		case Qt::DisplayRole:
		case Role::ItemID:
			return static_cast<qulonglong> (item.WID_);
		}

		return {};
	}

#if QT_VERSION < 0x050000
	template<>
	void TrayModel::HandleClientMsg<XClientMessageEvent*> (XClientMessageEvent *ev)
	{
		if (ev->message_type != Util::XWrapper::Instance ().GetAtom ("_NET_SYSTEM_TRAY_OPCODE"))
			return;

		switch (ev->data.l [1])
		{
		case 0:
			if (auto id = ev->data.l [2])
				Add (id);
		default:
			break;
		}
	}
#else
	template<>
	void TrayModel::HandleClientMsg<xcb_client_message_event_t*> (xcb_client_message_event_t *ev)
	{
		if (ev->type != Util::XWrapper::Instance ().GetAtom ("_NET_SYSTEM_TRAY_OPCODE"))
			return;

		switch (ev->data.data32 [1])
		{
		case 0:
			if (auto id = ev->data.data32 [2])
				Add (id);
		default:
			break;
		}
	}
#endif

	void TrayModel::Add (ulong wid)
	{
		if (FindItem (wid) != Items_.end ())
			return;

		if (Util::XWrapper::Instance ().IsLCWindow (wid))
			return;

		beginInsertRows ({}, Items_.size (), Items_.size ());
		Items_.append ({ wid });
		endInsertRows ();
	}

	void TrayModel::Remove (ulong wid)
	{
		const auto pos = FindItem (wid);
		if (pos == Items_.end ())
			return;

		const auto dist = std::distance (Items_.begin (), pos);
		beginRemoveRows ({}, dist, dist);
		Items_.erase (pos);
		endRemoveRows ();
	}

	void TrayModel::Update (ulong wid)
	{
		emit updateRequired (wid);
	}

#if QT_VERSION < 0x050000
	bool TrayModel::Filter (XEvent *ev)
	{
		auto invokePrev = [this, ev] { return PrevFilter_ ? PrevFilter_ (ev) : false; };

		if (!IsValid_)
			return invokePrev ();

		switch (ev->type)
		{
		case ClientMessage:
			HandleClientMsg (&ev->xclient);
			break;
		case DestroyNotify:
			Remove (ev->xany.window);
			break;
		default:
			if (ev->type == XDamageNotify + DamageEvent_)
			{
				auto dmg = reinterpret_cast<XDamageNotifyEvent*> (ev);
				Update (dmg->drawable);
			}
			break;
		}

		return invokePrev ();
	}
#else
	bool TrayModel::nativeEventFilter (const QByteArray& eventType, void *msg, long int*)
	{
		if (eventType != "xcb_generic_event_t")
			return false;

		const auto ev = static_cast<xcb_generic_event_t*> (msg);

		switch (ev->response_type & ~0x80)
		{
		case XCB_CLIENT_MESSAGE:
			HandleClientMsg (static_cast<xcb_client_message_event_t*> (msg));
			break;
		case XCB_DESTROY_NOTIFY:
			Remove (static_cast<xcb_destroy_notify_event_t*> (msg)->window);
			break;
		default:
			if (ev->response_type == XCB_DAMAGE_NOTIFY + DamageEvent_)
				break;
			break;
		}

		return false;
	}
#endif

	auto TrayModel::FindItem (ulong wid) -> QList<TrayItem>::iterator
	{
		return std::find_if (Items_.begin (), Items_.end (),
				[&wid] (const TrayItem& item) { return item.WID_ == wid; });
	}
}
}
