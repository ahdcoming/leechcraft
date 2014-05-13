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

#include "playerrulesmanager.h"
#include <QUrl>
#include <QStandardItemModel>
#include <interfaces/structures.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/an/ianemitter.h>
#include <interfaces/an/ianrulesstorage.h>
#include <interfaces/an/constants.h>
#include "core.h"
#include "player.h"

Q_DECLARE_METATYPE (QList<LeechCraft::Entity>)

namespace LeechCraft
{
namespace LMP
{
	PlayerRulesManager::PlayerRulesManager (QStandardItemModel *model, QObject *parent)
	: QObject { parent }
	, Model_ { model }
	{
		connect (model,
				SIGNAL (rowsInserted (QModelIndex, int, int)),
				this,
				SLOT (insertRows (QModelIndex, int, int)));
		connect (model,
				SIGNAL (rowsAboutToBeRemoved (QModelIndex, int, int)),
				this,
				SLOT (removeRows (QModelIndex, int, int)));
		connect (model,
				SIGNAL (modelReset ()),
				this,
				SLOT (handleReset ()));
	}

	namespace
	{
		bool MatchesRule (const MediaInfo& info, const Entity& rule)
		{
			auto url = info.Additional_.value ("URL").toUrl ();
			if (url.isEmpty ())
				url = QUrl::fromLocalFile (info.LocalPath_);

			const QList<QPair<QString, ANFieldValue>> fields
			{
				{
					AN::Field::MediaArtist,
					ANStringFieldValue { info.Artist_ }
				},
				{
					AN::Field::MediaAlbum,
					ANStringFieldValue { info.Album_ }
				},
				{
					AN::Field::MediaTitle,
					ANStringFieldValue { info.Title_ }
				},
				{
					AN::Field::MediaLength,
					ANIntFieldValue { info.Length_, ANIntFieldValue::OEqual }
				},
				{
					AN::Field::MediaPlayerURL,
					ANStringFieldValue { url.toEncoded () }
				}
			};

			const auto& map = rule.Additional_;
			bool hadAtLeastOne = false;
			for (const auto& field : fields)
			{
				if (!map.contains (field.first))
					continue;

				hadAtLeastOne = true;

				const auto& value = map.value (field.first).value<ANFieldValue> ();
				if (!(value == field.second))
					return false;
			}

			return hadAtLeastOne;
		}

		QList<Entity> FindMatching (const MediaInfo& info, const QList<Entity>& rules)
		{
			QList<Entity> result;
			for (const auto& rule : rules)
				if (MatchesRule (info, rule))
					result << rule;
			return result;
		}

		void ReapplyRules (const QList<QStandardItem*>& items, const QList<Entity>& rules)
		{
			for (const auto item : items)
			{
				const auto& info = item->data (Player::Role::Info).value<MediaInfo> ();

				const auto& matching = FindMatching (info, rules);
				item->setData (matching.isEmpty () ? QVariant {} : QVariant::fromValue (matching),
						Player::Role::MatchingRules);
			}
		}
	}

	void PlayerRulesManager::InitializePlugins ()
	{
		refillRules ();

		ReapplyRules (ManagedItems_, Rules_);
	}

	void PlayerRulesManager::insertRows (const QModelIndex& parent, int first, int last)
	{
		QList<QStandardItem*> list;
		for (int i = first; i <= last; ++i)
			list << Model_->itemFromIndex (Model_->index (i, 0, parent));

		QList<QStandardItem*> newItems;
		for (int i = 0; i < list.size (); ++i)
		{
			const auto item = list.at (i);

			if (!item->data (Player::IsAlbum).toBool ())
				newItems << item;

			for (int j = 0; j < item->rowCount (); ++j)
				list << item->child (j);
		}

		ReapplyRules (newItems, Rules_);

		ManagedItems_ += newItems;
	}

	void PlayerRulesManager::removeRows (const QModelIndex& parent, int first, int last)
	{
		QList<QStandardItem*> list;
		for (int i = first; i <= last; ++i)
			list << Model_->itemFromIndex (Model_->index (i, 0, parent));

		for (int i = 0; i < list.size (); ++i)
		{
			const auto item = list.at (i);

			ManagedItems_.removeOne (item);

			for (int j = 0; j < item->rowCount (); ++j)
				list << item->child (j);
		}
	}

	void PlayerRulesManager::handleReset ()
	{
		ManagedItems_.clear ();
		if (const auto rc = Model_->rowCount ())
			insertRows ({}, 0, rc - 1);
	}

	void PlayerRulesManager::refillRules ()
	{
		Rules_.clear ();

		const auto plugMgr = Core::Instance ().GetProxy ()->GetPluginsManager ();
		for (auto storage : plugMgr->GetAllCastableTo<IANRulesStorage*> ())
			Rules_ += storage->GetAllRules (AN::CatMediaPlayer);
	}
}
}
