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

#include "tabunhidelistview.h"
#if QT_VERSION < 0x050000
#include <QGraphicsObject>
#else
#include <QQuickItem>
#endif
#include <QtDebug>
#include <util/util.h>
#include <util/qml/unhidelistmodel.h>

namespace LeechCraft
{
namespace SB2
{
	TabUnhideListView::TabUnhideListView (const QList<TabClassInfo>& tcs,
			ICoreProxy_ptr proxy, QWidget *parent)
	: UnhideListViewBase (proxy,
		[&tcs] (QStandardItemModel *model)
		{
			for (const auto& tc : tcs)
			{
				auto item = new QStandardItem;
				item->setData (tc.TabClass_, Util::UnhideListModel::Roles::ItemClass);
				item->setData (tc.VisibleName_, Util::UnhideListModel::Roles::ItemName);
				item->setData (tc.Description_, Util::UnhideListModel::Roles::ItemDescription);
				item->setData (Util::GetAsBase64Src (tc.Icon_.pixmap (32, 32).toImage ()),
						Util::UnhideListModel::Roles::ItemIcon);
				model->appendRow (item);
			}
		},
		parent)
	{
		connect (rootObject (),
				SIGNAL (itemUnhideRequested (QString)),
				this,
				SLOT (unhide (QString)),
				Qt::QueuedConnection);
	}

	void TabUnhideListView::unhide (const QString& idStr)
	{
		const auto& id = idStr.toUtf8 ();
		emit unhideRequested (id);

		for (int i = 0; i < Model_->rowCount (); ++i)
			if (Model_->item (i)->data (Util::UnhideListModel::Roles::ItemClass).toByteArray () == id)
			{
				Model_->removeRow (i);
				break;
			}

		if (!Model_->rowCount ())
			deleteLater ();
	}
}
}
