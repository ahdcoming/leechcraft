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
#include <qwebview.h>
#include <interfaces/structures.h>
#include <interfaces/core/ihookproxy.h>
#include "interfaces/poshuku/poshukutypes.h"
#include "interfaces/poshuku/iwebview.h"
#include "pageformsdata.h"

class QTimer;
class QWebInspector;

class IEntityManager;

namespace LeechCraft
{
namespace Poshuku
{
	class IBrowserWidget;

	class WebViewSslWatcherHandler;

	class CustomWebView : public QWebView
						, public IWebView
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Poshuku::IWebView)

		IBrowserWidget *Browser_;
		QString PreviousEncoding_;

		std::shared_ptr<QWebInspector> WebInspector_;

		const WebViewSslWatcherHandler *SslWatcherHandler_;
	public:
		CustomWebView (IEntityManager*, QWidget* = nullptr);

		void SetBrowserWidget (IBrowserWidget*);
		void Load (const QUrl&, QString = QString ());
		void Load (const QNetworkRequest&,
				QNetworkAccessManager::Operation = QNetworkAccessManager::GetOperation,
				const QByteArray& = QByteArray ());

		/** This function is equivalent to url.toString() if the url is
		 * all in UTF-8. But if the site is in another encoding,
		 * QUrl::toString() returns a bad, unreadable and, moreover,
		 * unusable string. In this case, this function converts the url
		 * to its percent-encoding representation.
		 *
		 * @param[in] url The possibly non-UTF-8 URL.
		 * @return The \em url converted to Unicode.
		 */
		QString URLToProperString (const QUrl& url);

		void Print (bool preview);

		QWidget* GetQWidget () override;
		QList<QAction*> GetActions (ActionArea) const override;
		QUrl GetUrl () const override;
		void EvaluateJS (const QString&, const std::function<void (QVariant)>&) override;
	protected:
		void mousePressEvent (QMouseEvent*) override;
		void contextMenuEvent (QContextMenuEvent*) override;
		void keyReleaseEvent (QKeyEvent*) override;
	private:
		void NavigatePlugins ();
		void NavigateHome ();
		void PrintImpl (bool, QWebFrame*);
	private slots:
		void remakeURL (const QUrl&);
		void handleLoadFinished (bool);
		void handleFrameState (QWebFrame*, QWebHistoryItem*);
		void handlePrintRequested (QWebFrame*);
	signals:
		void urlChanged (const QString&);
		void closeRequested ();
		void storeFormData (const PageFormsData_t&);

		void navigateRequested (const QUrl&);

		void zoomChanged ();

		void contextMenuRequested (const QPoint& globalPos, const ContextMenuInfo&);
	};
}
}
