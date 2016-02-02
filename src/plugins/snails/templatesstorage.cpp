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

#include "templatesstorage.h"
#include <QtDebug>
#include <util/sys/paths.h>
#include <util/sll/either.h>
#include <interfaces/itexteditor.h>
#include "account.h"
#include "structures.h"

namespace LeechCraft
{
namespace Snails
{
	TemplatesStorage::TemplatesStorage ()
	: RootDir_ { Util::GetUserDir (Util::UserDir::LC, "snails/templates") }
	{
	}

	namespace
	{
		QString GetAccountDirName (const Account *acc)
		{
			return acc->GetID ();
		}

		template<typename F>
		auto WithFile (const QString& filename, QIODevice::OpenMode mode, F&& worker)
		{
			QFile file { filename };
			if (!file.open (QIODevice::ReadOnly))
			{
				const auto modeStr = mode == QIODevice::ReadOnly ? "for reading:" : "for writing:";
				qWarning () << Q_FUNC_INFO
						<< "cannot open file"
						<< file.fileName ()
						<< modeStr
						<< file.errorString ();
				const auto& msg = "Cannot open file " + file.fileName ().toStdString () +
						" " + modeStr + " " + file.errorString ().toStdString ();
				return std::result_of_t<F (QFile&)>::Left (std::runtime_error { msg });
			}

			return worker (file);
		}
	}

	auto TemplatesStorage::LoadTemplate (ContentType contentType,
			MsgType msgType, const Account *account) -> LoadResult_t
	{
		auto dir = RootDir_;
		if (account && !dir.cd (GetAccountDirName (account)))
			return LoadResult_t::Right ({});

		const auto& filename = GetBasename (msgType) + "." + GetExtension (contentType);
		if (!dir.exists (filename))
			return LoadResult_t::Right ({});

		return WithFile (dir.filePath (filename), QIODevice::ReadOnly,
				[] (QFile& file)
				{
					return LoadResult_t::Right (QString::fromUtf8 (file.readAll ()));
				});
	}

	auto TemplatesStorage::SaveTemplate (ContentType contentType,
			MsgType msgType, const Account *account, const QString& tpl) -> SaveResult_t
	{
		auto dir = RootDir_;
		if (account)
		{
			const auto& dirName = GetAccountDirName (account);
			if (!dir.exists (dirName) && !dir.mkdir (dirName))
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot create"
						<< dirName
						<< "in"
						<< dir;
				const auto& msg = "Cannot create " + dir.filePath (dirName).toStdString ();
				return SaveResult_t::Left (std::runtime_error { msg });
			}

			if (!dir.cd (account->GetID ()))
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot cd"
						<< dir
						<< "into"
						<< dirName;
				const auto& msg = "Cannot cd into " + dir.filePath (dirName).toStdString ();
				return SaveResult_t::Left (std::runtime_error { msg });
			}
		}

		return WithFile (dir.filePath (GetBasename (msgType) + "." + GetExtension (contentType)),
				QIODevice::WriteOnly,
				[&tpl] (QFile& file)
				{
					if (file.write (tpl.toUtf8 ()) == -1)
					{
						qWarning () << Q_FUNC_INFO
								<< "cannot save file"
								<< file.fileName ()
								<< ":"
								<< file.errorString ();
						const auto& msg = "Cannot save file " + file.fileName ().toStdString () +
								": " + file.errorString ().toStdString ();
						return SaveResult_t::Left (std::runtime_error { msg });
					}

					return SaveResult_t::Right ({});
				});
	}
}
}
