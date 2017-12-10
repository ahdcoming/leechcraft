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

#include "tracer.h"
#include "accountlogger.h"

namespace LeechCraft
{
namespace Snails
{
	Tracer::Tracer (std::atomic<uint64_t>& sent, std::atomic<uint64_t>& received,
			const QString& context, int connId, AccountLogger *logger)
	: Sent_ (sent)
	, Received_ (received)
	, AccLogger_ { logger }
	, Context_ { context }
	, ConnId_ { connId }
	{
	}

	void Tracer::traceReceiveBytes (const vmime::size_t count, const vmime::string& state)
	{
		Received_.fetch_add (count, std::memory_order_relaxed);

		AccLogger_->Log (Context_, ConnId_,
				QString { "received %1 bytes in state %2" }
					.arg (count)
					.arg (state.empty () ? "<null>" : state.c_str ()));
	}

	void Tracer::traceSendBytes (const vmime::size_t count, const vmime::string& state)
	{
		Sent_.fetch_add (count, std::memory_order_relaxed);

		AccLogger_->Log (Context_, ConnId_,
				QString { "sent %1 bytes in state %2" }
					.arg (count)
					.arg (state.empty () ? "<null>" : state.c_str ()));
	}

	void Tracer::traceReceive (const vmime::string& line)
	{
		AccLogger_->Log (Context_, ConnId_,
				QString { "received:\n%1" }
					.arg (line.c_str ()));
	}

	void Tracer::traceSend (const vmime::string& line)
	{
		AccLogger_->Log (Context_, ConnId_,
				QString { "sent:\n%1" }
					.arg (line.c_str ()));
	}
}
}
