/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2014  Georg Rudoy
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

#include "zalil.h"
#include <QIcon>
#include <QStandardItemModel>
#include "servicesmanager.h"
#include "pendinguploadbase.h"

namespace LeechCraft
{
namespace Zalil
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Manager_ = std::make_shared<ServicesManager> (proxy);
		connect (Manager_.get (),
				SIGNAL (fileUploaded (QString, QUrl)),
				this,
				SIGNAL (fileUploaded (QString, QUrl)));

		ReprModel_ = new QStandardItemModel { this };
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Zalil";
	}

	void Plugin::Release ()
	{
		Manager_.reset ();
	}

	QString Plugin::GetName () const
	{
		return "Zalil";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Uploads files to filebin services without registration, like dump.bitcheese.net.");
	}

	QIcon Plugin::GetIcon () const
	{
		return {};
	}

	QStringList Plugin::GetServiceVariants () const
	{
		return Manager_->GetNames ({});
	}

	void Plugin::UploadFile (const QString& filename, const QString& service)
	{
		const auto pending = Manager_->Upload (filename, service);
		if (!pending)
			return;

		ReprModel_->appendRow (pending->GetReprRow ());
	}

	QAbstractItemModel* Plugin::GetRepresentation () const
	{
		return ReprModel_;
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_zalil, LeechCraft::Zalil::Plugin);
