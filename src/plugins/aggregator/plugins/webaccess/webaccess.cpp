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

#include "webaccess.h"
#include <QIcon>
#include <interfaces/aggregator/iproxyobject.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <util/util.h>
#include <util/network/addressesmodelmanager.h>
#include "servermanager.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
namespace WebAccess
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		Util::AddressesModelManager::RegisterTypes ();

		AddrMgr_ = new Util::AddressesModelManager (&XmlSettingsManager::Instance (), 9001, this);

		Util::InstallTranslator ("aggregator_webaccess");

		XSD_.reset (new Util::XmlSettingsDialog);
		XSD_->RegisterObject (&XmlSettingsManager::Instance (), "aggregatorwebaccesssettings.xml");

		XSD_->SetDataSource ("AddressesDataView", AddrMgr_->GetModel ());
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Aggregator.WebAccess";
	}

	void Plugin::Release ()
	{
		SM_.reset ();
	}

	QString Plugin::GetName () const
	{
		return "Aggregator WebAccess";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Provides remote HTTP/Web access to Aggregator.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Aggregator.GeneralPlugin/1.0";
		return result;
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XSD_;
	}

	void Plugin::initPlugin (QObject *proxy)
	{
		try
		{
			SM_.reset (new ServerManager (qobject_cast<IProxyObject*> (proxy), Proxy_, AddrMgr_));
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_aggregator_webaccess, LeechCraft::Aggregator::WebAccess::Plugin);
