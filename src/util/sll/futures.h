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

#pragma once

#include <type_traits>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include "slotclosure.h"

namespace LeechCraft
{
namespace Util
{
	namespace detail
	{
		template<typename T>
		struct UnwrapFutureType;

		template<typename T>
		struct UnwrapFutureType<QFuture<T>>
		{
			typedef T type;
		};

		template<typename T>
		using UnwrapFutureType_t = typename UnwrapFutureType<T>::type;

		template<typename T>
		struct IsFuture
		{
			constexpr static bool Result_ = false;
		};

		template<typename T>
		struct IsFuture<QFuture<T>>
		{
			constexpr static bool Result_ = true;
		};

		template<typename RetType, typename ResultHandler>
		struct HandlerInvoker
		{
			void operator() (const ResultHandler& rh, QFutureWatcher<RetType> *watcher) const
			{
				rh (watcher->result ());
			}
		};

		template<typename ResultHandler>
		struct HandlerInvoker<void, ResultHandler>
		{
			void operator() (const ResultHandler& rh, QFutureWatcher<void>*) const
			{
				rh ();
			}
		};
	}

	/** @brief Runs a QFuture-returning function and feeding the future
	 * to a handler when it is ready.
	 *
	 * This function creates a <code>QFutureWatcher</code> of a type
	 * compatible with the QFuture type returned from the \em f, makes
	 * sure that \em rh handler is invoked when the future finishes,
	 * and then invokes the \em f with the given list of \em args (that
	 * may be empty).
	 *
	 * \em rh should accept a single argument of the same type \em T
	 * that is wrapped in a <code>QFuture</code> returned by the \em f
	 * (that is, \em f should return <code>QFuture<T></code>).
	 *
	 * @param[in] f A callable that should be executed, taking the
	 * arguments \em args and returning a <code>QFuture<T></code> for
	 * some \em T.
	 * @param[in] rh A callable that will be invoked when the future
	 * finishes, that should be callable with a single argument of type
	 * \em T.
	 * @param[in] parent The parent object for all QObject-derived
	 * classes created in this function, may be a <code>nullptr</code>.
	 * @param[in] args The arguments to be passed to the callable \em f.
	 */
	template<typename Executor, typename ResultHandler, typename... Args>
	void ExecuteFuture (Executor f, ResultHandler rh, QObject *parent, Args... args)
	{
		static_assert (detail::IsFuture<decltype (f (args...))>::Result_,
				"The passed functor should return a QFuture.");

		// Don't replace result_of with decltype, this triggers a gcc bug leading to segfault:
		// http://leechcraft.org:8080/job/leechcraft/=debian_unstable/1998/console
		using RetType_t = detail::UnwrapFutureType_t<typename std::result_of<Executor (Args...)>::type>;
		const auto watcher = new QFutureWatcher<RetType_t> { parent };

		new SlotClosure<DeleteLaterPolicy>
		{
			[watcher, rh] { detail::HandlerInvoker<RetType_t, ResultHandler> {} (rh, watcher); },
			watcher,
			SIGNAL (finished ()),
			watcher
		};

		watcher->setFuture (f (args...));
	}
}
}

