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

#include "audioaddictstreamfetcher.h"
#include <QNetworkRequest>
#include <QtDebug>
#include <util/sll/parsejson.h>

namespace LeechCraft
{
namespace HotStreams
{
	namespace
	{
		const QString APIUsername = "ephemeron";
		const QString APIPass = "dayeiph0ne@pp";

		QString Service2ID (AudioAddictStreamFetcher::Service service)
		{
			switch (service)
			{
			case AudioAddictStreamFetcher::Service::DI:
				return "di";
			case AudioAddictStreamFetcher::Service::SkyFM:
				return "sky";
			}

			qWarning () << Q_FUNC_INFO
					<< "unknown service"
					<< static_cast<int> (service);
			return QString ();
		}
	}

	AudioAddictStreamFetcher::AudioAddictStreamFetcher (Service service,
			QStandardItem *root, QNetworkAccessManager *nam, QObject *parent)
	: StreamListFetcherBase (root, nam, parent)
	, Service_ (service)
	{
		const auto& abbr = Service2ID (service);
		if (abbr.isEmpty ())
		{
			deleteLater ();
			return;
		}

		const auto& urlStr = QString ("http://api.v2.audioaddict.com/v1/%1/mobile/batch_update?asset_group_key=mobile_icons&stream_set_key=").arg (abbr);
		auto req = QNetworkRequest (QUrl (urlStr));
		req.setRawHeader("Authorization",
				"Basic " + QString ("%1:%2")
					.arg (APIUsername)
					.arg (APIPass)
					.toLatin1 ()
					.toBase64 ());
		Request (req);
	}

	QList<StreamListFetcherBase::StreamInfo> AudioAddictStreamFetcher::Parse (const QByteArray& data)
	{
		QList<StreamInfo> result;

		const auto& map = Util::ParseJson (data, Q_FUNC_INFO).toMap ();

		if (!map.contains ("channel_filters"))
			return result;

		Q_FOREACH (const auto& filterVar, map ["channel_filters"].toList ())
		{
			const auto& filter = filterVar.toMap ();
			if (filter ["name"].toString () != "All")
				continue;

			Q_FOREACH (const auto& channelVar, filter.value ("channels").toList ())
			{
				const auto& channel = channelVar.toMap ();

				const auto& key = channel ["key"].toString ();

				const QUrl url (QString ("http://listen.%1.fm/public3/%2.pls")
							.arg (Service2ID (Service_))
							.arg (key));
				StreamInfo info =
				{
					channel ["name"].toString (),
					channel ["description"].toString (),
					QStringList (),
					url,
					channel ["asset_url"].toString (),
					channel ["channel_director"].toString (),
					"pls"
				};
				result << info;
			}

			break;
		}

		return result;
	}
}
}
