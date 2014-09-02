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

#include "transcodejob.h"
#include <stdexcept>
#include <functional>
#include <QMap>
#include <QDir>
#include <QUuid>
#include <QtDebug>
#include <taglib/tag.h>
#include "transcodingparams.h"
#include "core.h"
#include "localfileresolver.h"

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		QString BuildTranscodedPath (const QString& path, const TranscodingParams& params)
		{
			QDir dir = QDir::temp ();
			if (!dir.exists ("lmp_transcode"))
				dir.mkdir ("lmp_transcode");
			if (!dir.cd ("lmp_transcode"))
				throw std::runtime_error ("unable to cd into temp dir");

			const QFileInfo fi (path);

			const auto format = Formats ().GetFormat (params.FormatID_);

			auto result = dir.absoluteFilePath (fi.fileName ());
			auto ext = format->GetFileExtension ();
			ext.prepend (QUuid::createUuid ().toString () + ".");
			const auto dotIdx = result.lastIndexOf ('.');
			if (dotIdx == -1)
				result += '.' + ext;
			else
				result.replace (dotIdx + 1, result.size () - dotIdx, ext);

			return result;
		}
	}

	TranscodeJob::TranscodeJob (const QString& path, const TranscodingParams& params, QObject* parent)
	: QObject (parent)
	, Process_ (new QProcess (this))
	, OriginalPath_ (path)
	, TranscodedPath_ (BuildTranscodedPath (path, params))
	, TargetPattern_ (params.FilePattern_)
	{
		QStringList args
		{
			"-i",
			path,
			"-vn"
		};
		args << Formats {}.GetFormat (params.FormatID_)->ToFFmpeg (params);
		args << TranscodedPath_;

		connect (Process_,
				SIGNAL (finished (int, QProcess::ExitStatus)),
				this,
				SLOT (handleFinished (int, QProcess::ExitStatus)));
		connect (Process_,
				SIGNAL (readyRead ()),
				this,
				SLOT (handleReadyRead ()));
		Process_->start ("ffmpeg", args);

#ifdef Q_OS_UNIX
		setpriority (PRIO_PROCESS, Process_->pid (), 19);
#endif
	}

	QString TranscodeJob::GetOrigPath () const
	{
		return OriginalPath_;
	}

	QString TranscodeJob::GetTranscodedPath () const
	{
		return TranscodedPath_;
	}

	QString TranscodeJob::GetTargetPattern () const
	{
		return TargetPattern_;
	}

	namespace
	{
		bool CheckTags (const TagLib::FileRef& ref, const QString& filename)
		{
			if (!ref.tag ())
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot get tags for"
						<< filename;
				return false;
			}

			return true;
		}

		void CopyTags (const QString& from, const QString& to)
		{
			const auto resolver = Core::Instance ().GetLocalFileResolver ();

			QMutexLocker locker (&resolver->GetMutex ());

			auto fromRef = resolver->GetFileRef (from);
			auto toRef = resolver->GetFileRef (to);

			if (!CheckTags (fromRef, from) || !CheckTags (toRef, to))
				return;

			TagLib::Tag::duplicate (fromRef.tag (), toRef.tag ());

			if (!toRef.save ())
				qWarning () << Q_FUNC_INFO
						<< "cannot save file"
						<< to;
		}
	}

	void TranscodeJob::handleFinished (int code, QProcess::ExitStatus status)
	{
		qDebug () << Q_FUNC_INFO << code << status;
		if (code)
			qWarning () << Q_FUNC_INFO
					<< Process_->readAllStandardError ();

		CopyTags (OriginalPath_, TranscodedPath_);

		emit done (this, !code);
	}

	void TranscodeJob::handleReadyRead ()
	{
	}
}
}
