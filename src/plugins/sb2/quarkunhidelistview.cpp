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

#include "quarkunhidelistview.h"
#include <QQuickItem>
#include <QtDebug>
#include <util/util.h>
#include <util/qml/unhidelistmodel.h>
#include "quarkmanager.h"
#include "viewmanager.h"

namespace LeechCraft
{
namespace SB2
{
	QuarkUnhideListView::QuarkUnhideListView (const QuarkComponents_t& components,
			ViewManager *viewMgr, ICoreProxy_ptr proxy, QWidget *parent)
	: Util::UnhideListViewBase (proxy,
			[&components, &proxy, viewMgr] (QStandardItemModel *model)
			{
				for (const auto& comp : components)
				{
					std::unique_ptr<QuarkManager> qm;
					try
					{
						qm.reset (new QuarkManager (comp, viewMgr, proxy));
					}
					catch (const std::exception& e)
					{
						qWarning () << Q_FUNC_INFO
								<< "error creating manager for quark"
								<< comp->Url_;
						continue;
					}

					const auto& manifest = qm->GetManifest ();

					auto item = new QStandardItem;
					item->setData (manifest.GetID (), Util::UnhideListModel::Roles::ItemClass);
					item->setData (manifest.GetName (), Util::UnhideListModel::Roles::ItemName);
					item->setData (manifest.GetDescription (), Util::UnhideListModel::Roles::ItemDescription);
					item->setData (Util::GetAsBase64Src (manifest.GetIcon ().pixmap (32, 32).toImage ()),
							Util::UnhideListModel::Roles::ItemIcon);
					model->appendRow (item);
				}
			},
			parent)
	, ViewManager_ (viewMgr)
	{
		for (const auto& comp : components)
		{
			try
			{
				const auto& manager = std::make_shared<QuarkManager> (comp, ViewManager_, proxy);
				const auto& manifest = manager->GetManifest ();
				ID2Component_ [manifest.GetID ()] = { comp, manager };
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "skipping component"
						<< comp->Url_
						<< ":"
						<< e.what ();
			}
		}

		connect (rootObject (),
				SIGNAL (itemUnhideRequested (QString)),
				this,
				SLOT (unhide (QString)),
				Qt::QueuedConnection);
	}

	void QuarkUnhideListView::unhide (const QString& itemClass)
	{
		const auto& info = ID2Component_.take (itemClass);
		ViewManager_->UnhideQuark (info.Comp_, info.Manager_);

		for (int i = 0; i < Model_->rowCount (); ++i)
			if (Model_->item (i)->data (Util::UnhideListModel::Roles::ItemClass) == itemClass)
			{
				Model_->removeRow (i);
				break;
			}
	}
}
}
