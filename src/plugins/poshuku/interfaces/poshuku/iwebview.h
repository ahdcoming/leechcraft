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

#include <memory>
#include <functional>
#include "poshukutypes.h"

class QUrl;
class QAction;
class QString;
class QWidget;

template<typename>
class QList;

namespace LeechCraft
{
namespace Poshuku
{
	class IWebViewHistory;
	using IWebViewHistory_ptr = std::shared_ptr<IWebViewHistory>;

	/** @brief Interface for QWebView-like widgets displaying HTML content.
	 *
	 * The implementations of this interface are also expected to have the
	 * following signals, following QWebView API:
	 * - loadStarted()
	 * - loadProgress(int)
	 * - loadFinished(bool)
	 * - iconChanged()
	 * - titleChanged(QString)
	 * - urlChanged(QUrl)
	 * - urlChanged(QString)
	 * - zoomChanged()
	 * - statusBarMessage(QString)
	 * - closeRequested()
	 */
	class IWebView
	{
	public:
		virtual ~IWebView () = default;

		enum class ActionArea
		{
			UrlBar
		};

		enum class PageAction
		{
			Reload,
			ReloadAndBypassCache,
			Stop,

			Back,
			Forward,

			Cut,
			Copy,
			Paste,

			CopyLinkToClipboard,
			DownloadLinkToDisk,

			OpenImageInNewWindow,
			DownloadImageToDisk,
			CopyImageToClipboard,
			CopyImageUrlToClipboard,

			InspectElement
		};

		virtual QWidget* GetQWidget () = 0;

		virtual QList<QAction*> GetActions (ActionArea) const = 0;

		virtual QAction* GetPageAction (PageAction) const = 0;

		virtual QString GetTitle () const = 0;
		virtual QUrl GetUrl () const = 0;
		virtual QString GetHumanReadableUrl () const = 0;
		virtual QIcon GetIcon () const = 0;

		virtual void Load (const QUrl& url, const QString& title = {}) = 0;

		virtual void SetContent (const QByteArray& data, const QByteArray& mime, const QUrl& base = {}) = 0;

		virtual QString ToHtml () const = 0;

		virtual void EvaluateJS (const QString& js,
				const std::function<void (QVariant)>& handler = {}) = 0;
		virtual void AddJavaScriptObject (const QString& id, QObject *object) = 0;

		virtual void Print (bool withPreview) = 0;

		virtual QPoint GetScrollPosition () const = 0;
		virtual void SetScrollPosition (const QPoint&) = 0;

		virtual double GetZoomFactor () const = 0;
		virtual void SetZoomFactor (double) = 0;

		virtual double GetTextSizeMultiplier () const = 0;
		virtual void SetTextSizeMultiplier (double) = 0;

		virtual QString GetDefaultTextEncoding () const = 0;
		virtual void SetDefaultTextEncoding (const QString& encoding) = 0;

		virtual IWebViewHistory_ptr GetHistory () = 0;
	protected:
		virtual void earliestViewLayout () = 0;

		virtual void linkHovered (const QString& link, const QString& title, const QString& textContent) = 0;

		virtual void storeFormData (const PageFormsData_t&) = 0;
	};
}
}

Q_DECLARE_INTERFACE (LeechCraft::Poshuku::IWebView,
		"org.LeechCraft.Poshuku.IWebView/1.0")
