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

#include "fua.h"
#include <QStandardItemModel>
#include <QUrl>
#include <QSettings>
#include <QCoreApplication>
#include <util/util.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <interfaces/poshuku/iproxyobject.h>
#include "settings.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Util
{
	class XmlSettingsDialog;
};
namespace Poshuku
{
namespace Fua
{
	namespace
	{
		QMap<QString, QString> MakeLookupMap (const QList<QPair<QString, QString>>& pairs)
		{
			QMap<QString, QString> result;
			for (const auto& pair : pairs)
				result [pair.second] = pair.first;
			return result;
		}
	}

	void FUA::Init (ICoreProxy_ptr)
	{
		Util::InstallTranslator ("poshuku_fua");

		Model_ = std::make_shared<QStandardItemModel> ();
		Model_->setHorizontalHeaderLabels ({ tr ("Domain"), tr ("Agent"), tr ("Identification string") });

		XmlSettingsDialog_ = std::make_shared<Util::XmlSettingsDialog> ();
		XmlSettingsDialog_->RegisterObject (XmlSettingsManager::Instance (),
				"poshukufuasettings.xml");
		XmlSettingsDialog_->SetCustomWidget ("Settings", new Settings { Model_.get (), this });
	}

	void FUA::SecondInit ()
	{
	}

	void FUA::Release ()
	{
	}

	QByteArray FUA::GetUniqueID () const
	{
		return "org.LeechCraft.Poshuku.FUA";
	}

	QString FUA::GetName () const
	{
		return "Poshuku FUA";
	}

	QString FUA::GetInfo () const
	{
		return tr ("Allows one to set fake user agents for different sites.");
	}

	QIcon FUA::GetIcon () const
	{
		static QIcon icon { "lcicons:/poshuku/fua/resources/images/poshuku_fua.svg" };
		return icon;
	}

	QSet<QByteArray> FUA::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Poshuku.Plugins/1.0";
		return result;
	}

	Util::XmlSettingsDialog_ptr FUA::GetSettingsDialog () const
	{
		return XmlSettingsDialog_;
	}

	void FUA::hookUserAgentForUrlRequested (LeechCraft::IHookProxy_ptr proxy,
			const QUrl& url)
	{
		const auto& host = url.host ();
		for (int i = 0; i < Model_->rowCount (); ++i)
		{
			const auto item = Model_->item (i);
			QRegExp re { item->text (), Qt::CaseSensitive, QRegExp::Wildcard };
			if (re.exactMatch (host))
			{
				proxy->CancelDefault ();
				proxy->SetReturnValue (Model_->item (i, 2)->text ());
				return;
			}
		}
	}

	void FUA::Save () const
	{
		QSettings settings { QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Poshuku_FUA" };
		settings.beginWriteArray ("Fakes");
		settings.remove ("");
		for (int i = 0; i < Model_->rowCount (); ++i)
		{
			settings.setArrayIndex (i);
			settings.setValue ("domain", Model_->item (i, 0)->text ());
			settings.setValue ("identification", Model_->item (i, 2)->text ());
		}
		settings.endArray ();
	}

	const QList<QPair<QString, QString>>& FUA::GetBrowser2ID () const
	{
		return Browser2ID_;
	}

	const QMap<QString, QString>& FUA::GetBackLookupMap () const
	{
		return BackLookup_;
	}

	void FUA::initPlugin (QObject*)
	{
		Browser2ID_ = QList<QPair<QString, QString>>
		{
			{ tr ("LeechCraft (this machine)"), "" },
			{ "Android Webkit Browser (Android 2.2.1, HTC Desire Z)", "Mozilla/5.0 (Linux; U; Android 2.2.1; en-gb; HTC_DesireZ_A7272 Build/FRG83D) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1" },
			{ "Android Webkit Browser (Android 4.0.3)", "Mozilla/5.0 (Linux; U; Android 4.0.3; de-ch; HTC Sensation Build/IML74K) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30" },
			{ "Chromium 28.0 (Linux x86)", "Mozilla/5.0 (X11; Linux i686) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/28.0.1500.71 Chrome/28.0.1500.71 Safari/537.36" },
			{ "Chromium 30.0 (Linux x86_64)", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.66 Safari/537.36" },
			{ "Chrome 30.0 (Windows 7)", "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36" },
			{ "Chrome 30.0 (Mac OS X 10.8)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.69 Safari/537.36" },
			{ "Chrome 41.0 (Mac OS X 10.10)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/41.0.2227.1 Safari/537.36" },
			{ "Epiphany 2.30.6 (Ubuntu 10.04)", "Mozilla/5.0 (X11; U; Linux x86_64; en_US) AppleWebKit/534.26+ (KHTML, like Gecko) Ubuntu/11.04 Epiphany/2.30.6" },
			{ "Firefox 24.0 (Linux x86_64)", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:24.0) Gecko/20100101 Firefox/24.0" },
			{ "Firefox 24.0 (Windows 7)", "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:24.0) Gecko/20100101 Firefox/24.0" },
			{ "Firefox 24.0 (Mac OS X 10.8)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.8; rv:24.0) Gecko/20100101 Firefox/24.0" },
			{ "Firefox 36.0 (Windows 8.1)", "Mozilla/5.0 (Windows NT 6.3; rv:36.0) Gecko/20100101 Firefox/36.0" },
			{ "IE 5.0 (Mac PPC)", "Mozilla/4.0 (compatible; MSIE 5.0; Mac_PowerPC)" },
			{ "IE 6.0 (Windows XP)", "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)" },
			{ "IE 8.0 (Windows XP SP3)", "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30; .NET CLR 3.0.04506.648; InfoPath.1; .NET CLR 3.5.21022; .NET CLR 1.1.4322; OfficeLiveConnector.1.4; OfficeLivePatch.1.3; .NET CLR 3.0.4506.2152)" },
			{ "IE 8.0 (Windows 7)", "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.3)" },
			{ "IE 9.0 (Windows 7)", "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/5.0)" },
			{ "IE 10.0 (Windows 7)", "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)" },
			{ "IE 11.0 (Windows 8.1)", "Mozilla/5.0 (compatible, MSIE 11, Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko" },
			{ "Opera 4.03 (Windows NT 4.0)", "Opera/4.03 (Windows NT 4.0; U)" },
			{ "Opera 12.16 (Windows 7)", "Opera/9.80 (Windows NT 6.1; WOW64) Presto/2.12.388 Version/12.16" },
			{ "Opera Mini 9.80 (S60)", "Opera/9.80 (J2ME/MIDP; Opera Mini/9.80 (S60; SymbOS; Opera Mobi/23.348; U; en) Presto/2.5.25 Version/10.54" },
			{ "Opera Mobile 12.02 (Android 4.1)", "Opera/12.02 (Android 4.1; Linux; Opera Mobi/ADR-1111101157; U; en-US) Presto/2.9.201 Version/12.02" },
			{ "Safari 2 (Mac OS X PowerPC)", "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; appLanguage) AppleWebKit/412 (KHTML, like Gecko) Safari/412" },
			{ "Safari 5 (Windows 7)", "Mozilla/5.0 (Windows; U; Windows NT 6.1; ru-RU) AppleWebKit/533.16 (KHTML, like Gecko) Version/5.0 Safari/533.16" },
			{ "Safari 6.0 (Mac OS X 10.8)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/536.30.1 (KHTML, like Gecko) Version/6.0.5 Safari/536.30.1" },
			{ "Safari 7.0 (Mac OS X 10.9)", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9) AppleWebKit/537.71 (KHTML, like Gecko) Version/7.0 Safari/537.71" },
			{ "UCWeb 7 (Windows Mobile 5)", "HTC_P3400-Mozilla/4.0 Mozilla/4.0 (compatible; MSIE 4.01; Windows CE; PPC)/UCWEB7.0.0.41/31/400" }
		};

		BackLookup_ = MakeLookupMap (Browser2ID_);

		QSettings settings { QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Poshuku_FUA" };
		int size = settings.beginReadArray ("Fakes");

		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex (i);
			const auto& domain = settings.value ("domain").toString ();
			const auto& identification = settings.value ("identification").toString ();
			const QList<QStandardItem*> items
			{
				new QStandardItem { domain },
				new QStandardItem { BackLookup_ [identification] },
				new QStandardItem { identification }
			};
			Model_->appendRow (items);
		}
		settings.endArray ();
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_poshuku_fua, LeechCraft::Poshuku::Fua::FUA);
