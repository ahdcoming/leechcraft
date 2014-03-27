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

#include "gdblauncher.h"
#include <stdexcept>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QtDebug>

namespace LeechCraft
{
namespace AnHero
{
namespace CrashProcess
{
	GDBLauncher::GDBLauncher (quint64 pid, const QString& path, QObject *parent)
	: QObject (parent)
	, Proc_ (new QProcess (this))
	{
		auto tmpFile = new QTemporaryFile ();
		if (!tmpFile->open ())
			throw std::runtime_error ("cannot open GDB temp file");

		QTextStream stream (tmpFile);
		stream << "thread\n"
				<< "thread apply all bt";

		Proc_->start ("gdb",
				{
					"-nw",
					"-n",
					"-batch",
					"-x",
					tmpFile->fileName (),
					"-p",
					QString::number (pid),
					path
				});
		connect (Proc_,
				SIGNAL (error (QProcess::ProcessError)),
				this,
				SLOT (handleError ()));
		connect (Proc_,
				SIGNAL (readyReadStandardOutput ()),
				this,
				SLOT (consumeStdout ()));
		connect (Proc_,
				SIGNAL (finished (int, QProcess::ExitStatus)),
				this,
				SIGNAL (finished (int, QProcess::ExitStatus)));
	}

	GDBLauncher::~GDBLauncher ()
	{
		if (Proc_->state () != QProcess::NotRunning)
		{
			Proc_->terminate ();
			if (!Proc_->waitForFinished (500))
				Proc_->kill ();
		}
	}

	void GDBLauncher::handleError ()
	{
		qDebug () << Q_FUNC_INFO
				<< "status:"
				<< Proc_->exitStatus ()
				<< "code:"
				<< Proc_->exitCode ()
				<< "error:"
				<< Proc_->error ()
				<< "str:"
				<< Proc_->errorString ();
	}

	void GDBLauncher::consumeStdout ()
	{
		const auto& str = Proc_->readAllStandardOutput ().trimmed ();
		emit gotOutput (str);
	}
}
}
}
