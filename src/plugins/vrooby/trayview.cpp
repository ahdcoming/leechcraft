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

#include "trayview.h"
#include <QSortFilterProxyModel>
#include <QIcon>

#if QT_VERSION < 0x050000
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeImageProvider>
#include <QGraphicsObject>
#else
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#endif

#include <QSortFilterProxyModel>
#include <QSettings>
#include <QCoreApplication>
#include <util/sys/paths.h>
#include <util/qml/themeimageprovider.h>
#include <util/qml/colorthemeproxy.h>
#include "flatmountableitems.h"
#include "devbackend.h"

namespace LeechCraft
{
namespace Vrooby
{
	class FilterModel : public QSortFilterProxyModel
	{
		bool FilterEnabled_;
		QSet<QString> Hidden_;
	public:
		FilterModel (QObject *parent)
		: QSortFilterProxyModel (parent)
		, FilterEnabled_ (true)
		{
			setDynamicSortFilter (true);

			QSettings settings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Vrooby");
			settings.beginGroup ("HiddenDevices");
			Hidden_ = settings.value ("List").toStringList ().toSet ();
			settings.endGroup ();
		}

		QVariant data (const QModelIndex& index, int role) const
		{
			if (role != FlatMountableItems::ToggleHiddenIcon)
				return QSortFilterProxyModel::data (index, role);

			const auto& id = index.data (CommonDevRole::DevPersistentID).toString ();
			return Hidden_.contains (id) ?
					"image://ThemeIcons/list-add" :
					"image://ThemeIcons/list-remove";
		}

		void ToggleHidden (const QString& id)
		{
			if (!Hidden_.remove (id))
				Hidden_ << id;

			QSettings settings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Vrooby");
			settings.beginGroup ("HiddenDevices");
			settings.setValue ("List", QStringList (Hidden_.toList ()));
			settings.endGroup ();

			if (FilterEnabled_)
				invalidateFilter ();
			else
			{
				for (int i = 0; i < rowCount (); ++i)
				{
					const auto& idx = sourceModel ()->index (i, 0);
					if (id != idx.data (CommonDevRole::DevPersistentID).toString ())
						continue;

					const auto& mapped = mapFromSource (idx);
					emit dataChanged (mapped, mapped);
				}
			}

			if (Hidden_.isEmpty ())
			{
				FilterEnabled_ = true;
				invalidateFilter ();
			}
		}

		int GetHiddenCount () const
		{
			return Hidden_.size ();
		}

		void ToggleFilter ()
		{
			FilterEnabled_ = !FilterEnabled_;
			invalidateFilter ();
		}
	protected:
		bool filterAcceptsRow (int row, const QModelIndex&) const
		{
			if (!FilterEnabled_)
				return true;

			const auto& idx = sourceModel ()->index (row, 0);
			const auto& id = idx.data (CommonDevRole::DevPersistentID).toString ();
			return !Hidden_.contains (id);
		}
	};

	TrayView::TrayView (ICoreProxy_ptr proxy)
	: CoreProxy_ (proxy)
	, Flattened_ (new FlatMountableItems (this))
	, Filtered_ (new FilterModel (this))
	, Backend_ (0)
	{
		Filtered_->setSourceModel (Flattened_);

		setStyleSheet ("background: transparent");
		setWindowFlags (Qt::ToolTip);
		setAttribute (Qt::WA_TranslucentBackground);

		setResizeMode (SizeRootObjectToView);
		setFixedSize (500, 250);

		engine ()->addImageProvider ("ThemeIcons", new Util::ThemeImageProvider (proxy));
		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			engine ()->addImportPath (cand);

		rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (proxy->GetColorThemeManager (), this));
		rootContext ()->setContextProperty ("devModel", Filtered_);
		rootContext ()->setContextProperty ("devicesLabelText", tr ("Removable devices"));
		rootContext ()->setContextProperty ("hasHiddenItems", Filtered_->GetHiddenCount ());
		setSource (Util::GetSysPathUrl (Util::SysPath::QML, "vrooby", "DevicesTrayView.qml"));

		connect (Flattened_,
				SIGNAL (rowsInserted (QModelIndex, int, int)),
				this,
				SIGNAL (hasItemsChanged ()));
		connect (Flattened_,
				SIGNAL (rowsRemoved (QModelIndex, int, int)),
				this,
				SIGNAL (hasItemsChanged ()));

		connect (rootObject (),
				SIGNAL (toggleHideRequested (QString)),
				this,
				SLOT (toggleHide (QString)));
		connect (rootObject (),
				SIGNAL (toggleShowHidden ()),
				this,
				SLOT (toggleShowHidden ()));
	}

	void TrayView::SetBackend (DevBackend *backend)
	{
		if (Backend_)
			disconnect (rootObject (),
					0,
					Backend_,
					0);

		Backend_ = backend;
		connect (rootObject (),
				SIGNAL (toggleMountRequested (QString)),
				Backend_,
				SLOT (toggleMount (QString)));

		Flattened_->SetSource (Backend_->GetDevicesModel ());
	}

	bool TrayView::HasItems () const
	{
		return Flattened_->rowCount ();
	}

	void TrayView::toggleHide (const QString& persId)
	{
		Filtered_->ToggleHidden (persId);

		rootContext ()->setContextProperty ("hasHiddenItems", Filtered_->GetHiddenCount ());
	}

	void TrayView::toggleShowHidden ()
	{
		Filtered_->ToggleFilter ();
	}
}
}
