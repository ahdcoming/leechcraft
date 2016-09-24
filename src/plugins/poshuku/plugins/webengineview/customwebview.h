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

#include <QWebEngineView>
#include <interfaces/poshuku/iwebview.h>

namespace LeechCraft
{
namespace Poshuku
{
namespace WebEngineView
{
	class CustomWebView : public QWebEngineView
						, public IWebView
	{
		Q_OBJECT
	public:
		void SurroundingsInitialized () override;

		QWidget* GetQWidget () override;

		QList<QAction*> GetActions (ActionArea area) const override;
		QAction* GetPageAction (PageAction action) const override;

		QString GetTitle () const override;
		QUrl GetUrl () const override;
		QString GetHumanReadableUrl () const override;
		QIcon GetIcon () const override;

		void Load (const QUrl& url, const QString& title) override;

		void SetContent (const QByteArray& data, const QByteArray& mime, const QUrl& base) override;
		QString ToHtml () const override;

		void EvaluateJS (const QString& js, const std::function<void (QVariant)>& handler) override;
		void AddJavaScriptObject (const QString& id, QObject* object) override;

		void Print (bool withPreview) override;
		QPixmap MakeFullPageSnapshot () override;

		QPoint GetScrollPosition () const override;
		void SetScrollPosition (const QPoint& point) override;
		double GetZoomFactor () const override;
		void SetZoomFactor (double d) override;
		double GetTextSizeMultiplier () const override;
		void SetTextSizeMultiplier (double d) override;

		QString GetDefaultTextEncoding () const override;
		void SetDefaultTextEncoding (const QString& encoding) override;

		void InitiateFind (const QString& text) override;

		QMenu* CreateStandardContextMenu () override;

		IWebViewHistory_ptr GetHistory () override;

		void SetAttribute (Attribute attribute, bool b) override;
	signals:
		void earliestViewLayout () override;
		void linkHovered (const QString&, const QString&, const QString&) override;
		void storeFormData (const PageFormsData_t&) override;
		void featurePermissionRequested (const IWebView::IFeatureSecurityOrigin_ptr&, IWebView::Feature) override;
	};
}
}
}
