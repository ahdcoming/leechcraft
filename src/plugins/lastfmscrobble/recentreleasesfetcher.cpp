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

#include "recentreleasesfetcher.h"
#include <algorithm>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtDebug>
#include <QDomDocument>
#include <util/threads/futures.h>
#include <util/network/handlenetworkreply.h>
#include <util/sll/visitor.h>
#include <interfaces/media/irecentreleases.h>
#include "xmlsettingsmanager.h"
#include "util.h"

namespace LeechCraft
{
namespace Lastfmscrobble
{
	RecentReleasesFetcher::RecentReleasesFetcher (bool withRecs, QNetworkAccessManager *nam, QObject *parent)
	: QObject (parent)
	{
		Promise_.reportStarted ();

		const auto& user = XmlSettingsManager::Instance ()
				.property ("lastfm.login").toString ();
		const QList<QPair<QString, QString>> params
		{
			{ "user", user },
			{ "userecs", withRecs ? "1" : "0" }
		};
		Util::Sequence (this, Util::HandleReply (Request ("user.getNewReleases", nam, params), this)) >>
				Util::Visitor
				{
					[this] (Util::Void) { Util::ReportFutureResult (Promise_, QString { "Unable to send network request." }); },
					[this] (const QByteArray& data) { HandleData (data); }
				}.Finally ([this] { deleteLater (); });
	}

	QFuture<Media::IRecentReleases::Result_t> RecentReleasesFetcher::GetFuture ()
	{
		return Promise_.future ();
	}

	void RecentReleasesFetcher::HandleData (const QByteArray& data)
	{
		QDomDocument doc;
		if (!doc.setContent (data))
		{
			qWarning () << Q_FUNC_INFO
					<< "error parsing reply";
			Util::ReportFutureResult (Promise_, QString { "Error parsing reply." });
			return;
		}

		const auto& docElem = doc.documentElement ();
		if (docElem.attribute ("status") != "ok")
		{
			qWarning () << Q_FUNC_INFO
					<< "reply is not ok:"
					<< docElem.attribute ("status");
			Util::ReportFutureResult (Promise_, QString { "Error parsing reply." });
			return;
		}

		QList<Media::AlbumRelease> releases;

		const auto months = { "Jan", "Feb", "Mar",
				"Apr", "May", "Jun",
				"Jul", "Aug", "Sep",
				"Oct", "Nov", "Dec" };
		const auto monthsBegin = months.begin ();
		const auto monthsEnd = months.end ();
		auto album = docElem.firstChildElement ("albums").firstChildElement ("album");
		while (!album.isNull ())
		{
			const auto& strs = album.attribute ("releasedate").split (' ', QString::SkipEmptyParts);
			const int day = strs.value (1).toInt ();
			const int month = std::distance (monthsBegin,
						std::find (monthsBegin, monthsEnd, strs.value (2))) + 1;
			const int year = strs.value (3).toInt ();

			const QUrl& thumb = GetImage (album, "large");
			const QUrl& full = GetImage (album, "extralarge");

			Media::AlbumRelease release =
			{
				album.firstChildElement ("name").text (),
				album.firstChildElement ("artist").firstChildElement ("name").text (),
				QDateTime (QDate (year, month, day)),
				thumb,
				full,
				QUrl (album.firstChildElement ("url").text ())
			};
			releases << release;

			album = album.nextSiblingElement ("album");
		}

		Util::ReportFutureResult (Promise_, releases);
		emit gotRecentReleases (releases);
	}
}
}
