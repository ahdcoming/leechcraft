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

#include "itemssortfilterproxymodel.h"
#include <QtDebug>
#include <QTimer>
#include "modelroles.h"

namespace LeechCraft
{
namespace Launchy
{
	ItemsSortFilterProxyModel::ItemsSortFilterProxyModel (QAbstractItemModel *source, QObject *parent)
	: RoleNamesMixin<QSortFilterProxyModel> (parent)
	{
		setDynamicSortFilter (true);
		setSourceModel (source);
		setRoleNames (source->roleNames ());
		sort (0, Qt::AscendingOrder);
	}

	QString ItemsSortFilterProxyModel::GetAppFilterText () const
	{
		return AppFilterText_;
	}

	void ItemsSortFilterProxyModel::SetAppFilterText (const QString& text)
	{
		AppFilterText_ = text;
		QTimer::singleShot (0,
				this,
				SLOT (invalidateFilterSlot ()));
	}

	bool ItemsSortFilterProxyModel::lessThan (const QModelIndex& left, const QModelIndex& right) const
	{
		if (!AppFilterText_.isEmpty () || CategoryNames_ != QStringList ("X-Recent"))
			return QSortFilterProxyModel::lessThan (left, right);

		const auto leftPos = left.data (ModelRoles::ItemRecentPos).toInt ();
		const auto rightPos = right.data (ModelRoles::ItemRecentPos).toInt ();
		return leftPos < rightPos;
	}

	bool ItemsSortFilterProxyModel::filterAcceptsRow (int row, const QModelIndex&) const
	{
		const auto& idx = sourceModel ()->index (row, 0);

		if (AppFilterText_.isEmpty ())
		{
			if (CategoryNames_.isEmpty ())
				return false;

			if (CategoryNames_.contains ("X-Favorites") &&
					idx.data (ModelRoles::IsItemFavorite).toBool ())
				return true;

			if (CategoryNames_.contains ("X-Recent") &&
					idx.data (ModelRoles::IsItemRecent).toBool ())
				return true;

			const auto& itemCats = idx.data (ModelRoles::ItemNativeCategories).toStringList ();
			return std::find_if (CategoryNames_.begin (), CategoryNames_.end (),
					[&itemCats] (const QString& cat)
						{ return itemCats.contains (cat); }) != CategoryNames_.end ();
		}

		auto checkStr = [&idx, this] (int role)
		{
			return idx.data (role).toString ().contains (AppFilterText_, Qt::CaseInsensitive);
		};

		return checkStr (ModelRoles::ItemName) ||
				checkStr (ModelRoles::ItemDescription) ||
				checkStr (ModelRoles::ItemCommand);
	}

	void ItemsSortFilterProxyModel::setCategoryNames (const QStringList& cats)
	{
		CategoryNames_ = cats;
		invalidateFilter ();
	}

	void ItemsSortFilterProxyModel::invalidateFilterSlot ()
	{
		invalidateFilter ();
	}
}
}
