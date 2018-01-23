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

#pragma once

#include <boost/variant.hpp>
#include <QString>
#include <QList>
#include <QUrl>
#include <util/sll/eitherfwd.h>
#include "audiostructs.h"

template<typename>
class QFuture;

namespace Media
{
	/** @brief Contains information about a hyped artist.
	 *
	 * This structure is used to describe additional information about a
	 * hyped artist, like playcount or change in popularity.
	 *
	 * @sa IHypesProvider
	 */
	struct HypedArtistInfo
	{
		/** @brief Basic information about the artist.
		 *
		 * Contains basic common information about the artist, like name,
		 * description and tags.
		 */
		ArtistInfo Info_;

		/** @brief Change of popularity in percents.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if percentage change is unknown.
		 */
		int PercentageChange_;

		/** @brief Play count.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if play count is unknown.
		 */
		int Playcount_;

		/** @brief Number of listeners.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if listeners count is unknown.
		 */
		int Listeners_;
	};

	/** @brief Contains information about a hyped track.
	 *
	 * This structure is used to describe additional information about a
	 * hyped track, like playcount or change in popularity.
	 *
	 * @sa IHypesProvider
	 */
	struct HypedTrackInfo
	{
		/** @brief Name of the track.
		 */
		QString TrackName_;

		/** @brief Address of the track page.
		 *
		 * This field is expected to contain the address of the track on
		 * the service this HypedTrackInfo is got from, not the artist
		 * web site.
		 */
		QUrl TrackPage_;

		/** @brief Change of popularity in percents.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if percentage change is unknown.
		 */
		int PercentageChange_;

		/** @brief Play count.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if play count is unknown.
		 */
		int Playcount_;

		/** @brief Number of listeners.
		 *
		 * The period of time is unspecified, different services may
		 * choose to use different measures.
		 *
		 * This may be 0 if listeners count is unknown.
		 */
		int Listeners_;

		/** @brief Duration of the track.
		 */
		int Duration_;

		/** @brief URL of thumb image of this track or performing artist.
		 */
		QUrl Image_;

		/** @brief Full size image of this track or performing artist.
		 */
		QUrl LargeImage_;

		/** @brief Name of the performer of this track.
		 */
		QString ArtistName_;

		/** @brief URL of the artist page.
		 *
		 * This field is expected to contain the address of the artist on
		 * the service this HypedTrackInfo is got from, not the artist
		 * web site.
		 */
		QUrl ArtistPage_;
	};

	using HypedInfo_t = boost::variant<QList<HypedArtistInfo>, QList<HypedTrackInfo>>;

	/** @brief Interface for plugins that support fetching hypes.
	 *
	 * Hypes are either popular tracks and artists or those who gain
	 * a lot of popularity right now.
	 */
	class Q_DECL_EXPORT IHypesProvider
	{
	public:
		virtual ~IHypesProvider () {}

		/** @brief The result of a hyped entity list query result.
		 *
		 * The result of a hyped entity list query is either a string with a
		 * human-readable error text, or the list of the hyped items.
		 */
		using HypeQueryResult_t = LeechCraft::Util::Either<QString, HypedInfo_t>;

		/** @brief Returns the service name.
		 *
		 * This string returns a human-readable string with the service
		 * name, like "Last.FM".
		 *
		 * @return The human-readable service name.
		 */
		virtual QString GetServiceName () const = 0;

		/** @brief The type of the hype.
		 */
		enum class HypeType
		{
			/** @brief New artists rapidly growing in popularity.
			 */
			NewArtists,

			/** @brief New tracks rapidly growing in popularity.
			 */
			NewTracks,

			/** @brief Top artists.
			 */
			TopArtists,

			/** @brief Top tracks.
			 */
			TopTracks
		};

		/** @brief Returns whether the service supports the given hype type.
		 *
		 * @param[in] hype The hype type to query.
		 * @return Whether the service supports this hype type.
		 */
		virtual bool SupportsHype (HypeType hype) = 0;

		/** @brief Updates the list of hyped artists of the given type.
		 *
		 * @param[in] type The type of the hype to update.
		 * @return The future holding the hype query result.
		 */
		virtual QFuture<HypeQueryResult_t> RequestHype (HypeType type) = 0;
	};

	template<IHypesProvider::HypeType HypeType>
	auto GetHypedInfo (const HypedInfo_t& info)
	{
		if constexpr (HypeType == IHypesProvider::HypeType::NewArtists ||
				HypeType == IHypesProvider::HypeType::TopArtists)
			return boost::get<QList<HypedArtistInfo>> (info);
		else
			return boost::get<QList<HypedTrackInfo>> (info);
	}
}

Q_DECLARE_INTERFACE (Media::IHypesProvider, "org.LeechCraft.Media.IHypesProvider/1.0")
