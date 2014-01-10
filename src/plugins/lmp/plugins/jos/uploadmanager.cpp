/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "uploadmanager.h"
#include <QtDebug>
#include <interfaces/lmp/iunmountablesync.h>
#include "connection.h"

namespace LeechCraft
{
namespace LMP
{
namespace jOS
{
	UploadManager::UploadManager (QObject *parent)
	: QObject { parent }
	{
	}

	void UploadManager::SetInfo (const QString& origLocalPath, const UnmountableFileInfo& info)
	{
		Infos_ [origLocalPath] = info;
	}

	void UploadManager::Upload (const QString& localPath, const QString& origLocalPath, const QByteArray& to)
	{
		if (!AvailableConnections_.contains (to))
			try
			{
				const auto& conn = std::make_shared<Connection> (to);
				AvailableConnections_ [to] = conn;
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< e.what ();
				emit uploadFinished (localPath,
						QFile::OpenError,
						 tr ("Unable to contact the device: %1.").arg (e.what ()));
				return;
			}
	}
}
}
}
