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

#include "customwebpage.h"
#include <algorithm>
#include <QtDebug>
#include <QFile>
#include <QBuffer>
#include <qwebframe.h>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDesktopServices>
#include <QSysInfo>
#include <qwebelement.h>
#include <qwebhistory.h>
#include <util/xpc/util.h>
#include <util/xpc/defaulthookproxy.h>
#include <util/sll/slotclosure.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ientitymanager.h>
#include <interfaces/core/iiconthememanager.h>
#include "xmlsettingsmanager.h"
#include "customwebview.h"
#include "core.h"
#include "pluginmanager.h"
#include "jsproxy.h"
#include "externalproxy.h"
#include "webpluginfactory.h"
#include "browserwidget.h"
#include "featurepermnotification.h"

Q_DECLARE_METATYPE (QVariantList*);
Q_DECLARE_METATYPE (QNetworkReply*);

namespace LeechCraft
{
namespace Poshuku
{
	CustomWebPage::CustomWebPage (QObject *parent)
	: QWebPage (parent)
	, MouseButtons_ (Qt::NoButton)
	, Modifiers_ (Qt::NoModifier)
	, JSProxy_ (new JSProxy (this))
	, ExternalProxy_ (new ExternalProxy (this))
	{
		Core::Instance ().GetPluginManager ()->RegisterHookable (this);

		{
			Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
			emit hookWebPageConstructionBegin (proxy, this);
			if (proxy->IsCancelled ())
				return;
		}

		setForwardUnsupportedContent (true);

		setNetworkAccessManager (Core::Instance ().GetNetworkAccessManager ());
		setPluginFactory (Core::Instance ().GetWebPluginFactory ());

		connect (this,
				SIGNAL (delayedFillForms (QWebFrame*)),
				this,
				SLOT (fillForms (QWebFrame*)),
				Qt::QueuedConnection);

		connect (ExternalProxy_.get (),
				SIGNAL (gotEntity (const LeechCraft::Entity&)),
				this,
				SIGNAL (gotEntity (const LeechCraft::Entity&)));

		connect (mainFrame (),
				SIGNAL (javaScriptWindowObjectCleared ()),
				this,
				SLOT (handleJavaScriptWindowObjectCleared ()));
		connect (mainFrame (),
				SIGNAL (urlChanged (const QUrl&)),
				this,
				SIGNAL (loadingURL (const QUrl&)));
		connect (mainFrame (),
				SIGNAL (initialLayoutCompleted ()),
				this,
				SLOT (handleInitialLayoutCompleted ()));
		connect (this,
				SIGNAL (contentsChanged ()),
				this,
				SLOT (handleContentsChanged ()));
		connect (this,
				SIGNAL (databaseQuotaExceeded (QWebFrame*, QString)),
				this,
				SLOT (handleDatabaseQuotaExceeded (QWebFrame*, QString)));
		connect (this,
				SIGNAL (downloadRequested (const QNetworkRequest&)),
				this,
				SLOT (handleDownloadRequested (const QNetworkRequest&)));
		connect (this,
				SIGNAL (frameCreated (QWebFrame*)),
				this,
				SLOT (handleFrameCreated (QWebFrame*)));
		connect (this,
				SIGNAL (geometryChangeRequested (const QRect&)),
				this,
				SLOT (handleGeometryChangeRequested (const QRect&)));
		connect (this,
				SIGNAL (linkClicked (const QUrl&)),
				this,
				SLOT (handleLinkClicked (const QUrl&)));
		connect (this,
				SIGNAL (linkHovered (const QString&,
						const QString&, const QString&)),
				this,
				SLOT (handleLinkHovered (const QString&,
						const QString&, const QString&)));
		connect (this,
				SIGNAL (loadFinished (bool)),
				this,
				SLOT (handleLoadFinished (bool)));
		connect (this,
				SIGNAL (loadStarted ()),
				this,
				SLOT (handleLoadStarted ()));
		connect (this,
				SIGNAL (unsupportedContent (QNetworkReply*)),
				this,
				SLOT (handleUnsupportedContent (QNetworkReply*)));
		connect (this,
				SIGNAL (windowCloseRequested ()),
				this,
				SLOT (handleWindowCloseRequested ()));

		connect (this,
				SIGNAL (featurePermissionRequested (QWebFrame*, QWebPage::Feature)),
				this,
				SLOT (handleFeaturePermissionReq (QWebFrame*, QWebPage::Feature)));

		FillErrorSuggestions ();

		{
			Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
			emit hookWebPageConstructionEnd (proxy, this);
			if (proxy->IsCancelled ())
				return;
		}
	}

	CustomWebPage::~CustomWebPage ()
	{
	}

	void CustomWebPage::SetButtons (Qt::MouseButtons buttons)
	{
		MouseButtons_ = buttons;
	}

	void CustomWebPage::SetModifiers (Qt::KeyboardModifiers modifiers)
	{
		Modifiers_ = modifiers;
	}

	bool CustomWebPage::supportsExtension (QWebPage::Extension e) const
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
		emit hookSupportsExtension (proxy, this, e);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toBool ();

		switch (e)
		{
			case ErrorPageExtension:
				return true;
			default:
				return QWebPage::supportsExtension (e);
		}
	}

	bool CustomWebPage::extension (QWebPage::Extension e,
			const QWebPage::ExtensionOption* eo, QWebPage::ExtensionReturn *er)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
		emit hookExtension (proxy, this, e, eo, er);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toBool ();

		switch (e)
		{
			case ErrorPageExtension:
			{
				auto error = static_cast<const ErrorPageExtensionOption*> (eo);
				auto ret = static_cast<ErrorPageExtensionReturn*> (er);

				qDebug () << Q_FUNC_INFO
						<< "error extension:"
						<< error->domain
						<< error->error
						<< error->errorString
						<< error->url;

				switch (error->error)
				{
				case 102:			// Delegated entity
					return false;
				case 301:			// Unknown protocol (should delegate)
				{
					auto e = Util::MakeEntity (error->url,
							QString (),
							LeechCraft::FromUserInitiated);
					e.Additional_ ["IgnorePlugins"] = "org.LeechCraft.Poshuku";
					auto em = Core::Instance ().GetProxy ()->GetEntityManager ();
					if (em->CouldHandle (e))
					{
						em->HandleEntity (e);
						if (XmlSettingsManager::Instance ()->
								property ("CloseEmptyDelegatedPages").toBool () &&
								history ()->currentItem ().url ().isEmpty ())
							emit windowCloseRequested ();
						return false;
					}
				}
				default:
				{
					QString data = MakeErrorReplyContents (error->error,
							error->url, error->errorString, error->domain);
					ret->baseUrl = error->url;
					ret->content = data.toUtf8 ();
					if (error->domain == QWebPage::QtNetwork)
						switch (error->error)
						{
						case QNetworkReply::UnknownNetworkError:
							return QWebPage::extension (e, eo, er);
						case QNetworkReply::ContentReSendError:
							emit gotEntity (Util::MakeNotification ("Poshuku",
										tr ("Unable to send the request to %1. Please try submitting it again.")
											.arg (error->url.host ()),
										PCritical_));
							return false;
						default :
							return true;
						}
					else
						return true;
				}
				}
			}
			default:
				return QWebPage::extension (e, eo, er);
		}
	}

	namespace
	{
		QString GetFeatureText (QWebPage::Feature feature)
		{
			switch (feature)
			{
			case QWebPage::Notifications:
				return CustomWebPage::tr ("%1 requests access to notifications.");
			case QWebPage::Geolocation:
				return CustomWebPage::tr ("%1 requests access to geolocation services.");
			}

			return {};
		}
	}

	void CustomWebPage::handleFeaturePermissionReq (QWebFrame *frame, Feature feature)
	{
		const auto& text = GetFeatureText (feature)
				.arg (frame->url ().host ());
		qDebug () << Q_FUNC_INFO << view () << text;
		if (text.isEmpty ())
			return;

		const auto notification = new FeaturePermNotification { text, view () };
		notification->show ();

		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, notification, frame, feature] () -> void
			{
				setFeaturePermission (frame, feature, PermissionGrantedByUser);
				notification->deleteLater ();
			},
			notification,
			SIGNAL (granted ()),
			notification
		};
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, notification, frame, feature] () -> void
			{
				setFeaturePermission (frame, feature, PermissionDeniedByUser);
				notification->deleteLater ();
			},
			notification,
			SIGNAL (denied ()),
			notification
		};
	}

	void CustomWebPage::handleContentsChanged ()
	{
		emit hookContentsChanged (IHookProxy_ptr (new Util::DefaultHookProxy),
				this);
	}

	void CustomWebPage::handleDatabaseQuotaExceeded (QWebFrame *frame, QString string)
	{
		emit hookDatabaseQuotaExceeded (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, frame, string);
	}

	void CustomWebPage::handleDownloadRequested (const QNetworkRequest& other)
	{
		QNetworkRequest request = other;
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookDownloadRequested (proxy, this, request);
		if (proxy->IsCancelled ())
			return;

		proxy->FillValue ("request", request);

		Entity e = Util::MakeEntity (request.url (),
				QString (),
				FromUserInitiated);
		e.Additional_ ["AllowedSemantics"] = QStringList ("fetch") << "save";
		e.Additional_ ["IgnorePlugins"] = "org.LeechCraft.Poshuku";
		emit gotEntity (e);
	}

	void CustomWebPage::handleFrameCreated (QWebFrame *frame)
	{
		emit hookFrameCreated (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, frame);
	}

	void CustomWebPage::handleInitialLayoutCompleted ()
	{
		emit hookInitialLayoutCompleted (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, mainFrame ());
	}

	void CustomWebPage::handleJavaScriptWindowObjectCleared ()
	{
		QWebFrame *frame = qobject_cast<QWebFrame*> (sender ());
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
		emit hookJavaScriptWindowObjectCleared (proxy, this, frame);
		if (proxy->IsCancelled ())
			return;

		frame->addToJavaScriptWindowObject ("JSProxy", JSProxy_.get ());
		frame->addToJavaScriptWindowObject ("external", ExternalProxy_.get ());
		frame->evaluateJavaScript (R"delim(
			if (!Function.prototype.bind) {
			Function.prototype.bind = function (oThis) {
				if (typeof this !== "function") {
				// closest thing possible to the ECMAScript 5 internal IsCallable function
				throw new TypeError("Function.prototype.bind - what is trying to be bound is not callable");
				}

				var aArgs = Array.prototype.slice.call(arguments, 1),
					fToBind = this,
					fNOP = function () {},
					fBound = function () {
					return fToBind.apply(this instanceof fNOP && oThis
											? this
											: oThis,
										aArgs.concat(Array.prototype.slice.call(arguments)));
					};

				fNOP.prototype = this.prototype || {};
				fBound.prototype = new fNOP();

				return fBound;
			};
			}
		)delim");
	}

	void CustomWebPage::handleGeometryChangeRequested (const QRect& rect)
	{
		emit hookGeometryChangeRequested (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, rect);
	}

	void CustomWebPage::handleLinkClicked (const QUrl& url)
	{
		emit loadingURL (url);
		emit hookLinkClicked (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, url);
	}

	void CustomWebPage::handleLinkHovered (const QString& link,
			const QString& title, const QString& context)
	{
		emit hookLinkHovered (IHookProxy_ptr (new Util::DefaultHookProxy),
				this, link, title, context);
	}

	void CustomWebPage::handleLoadFinished (bool ok)
	{
		QWebElement body = mainFrame ()->findFirstElement ("body");

		if (body.findAll ("*").count () == 1 &&
				body.firstChild ().tagName () == "IMG")
			mainFrame ()->evaluateJavaScript ("function centerImg() {"
					"var img = document.querySelector('img');"
					"img.style.left = Math.floor((document.width - img.width) / 2) + 'px';"
					"img.style.top =  Math.floor((document.height - img.height) / 2) + 'px';"
					"img.style.position = 'absolute';"
					"}"
					"window.addEventListener('resize', centerImg, false);"
					"centerImg();");

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy ());
		emit hookLoadFinished (proxy, this, ok);
		if (proxy->IsCancelled ())
			return;

		emit delayedFillForms (mainFrame ());
	}

	void CustomWebPage::handleLoadStarted ()
	{
		emit hookLoadStarted (IHookProxy_ptr (new Util::DefaultHookProxy),
				this);
	}

	void CustomWebPage::handleUnsupportedContent (QNetworkReply *reply)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookUnsupportedContent (proxy, this, reply);
		if (proxy->IsCancelled ())
			return;

		std::shared_ptr<void> replyGuard = std::shared_ptr<void> (nullptr,
				[reply] (void*) -> void
				{
					reply->abort ();
					reply->deleteLater ();
				});

		const auto& url = reply->url ();
		const auto& mime = reply->header (QNetworkRequest::ContentTypeHeader).toString ();
		const auto& referer = reply->request ().rawHeader ("Referer");

		auto sendEnt = [reply, mime, referer, this] () -> void
		{
			auto e = Util::MakeEntity (reply->url (),
					{},
					LeechCraft::FromUserInitiated,
					mime);
			e.Additional_ ["IgnorePlugins"] = "org.LeechCraft.Poshuku";
			e.Additional_ ["Referer"] = QUrl::fromEncoded (referer);
			e.Additional_ ["Operation"] = reply->operation ();
			emit gotEntity (e);

			if (XmlSettingsManager::Instance ()->
					property ("CloseEmptyDelegatedPages").toBool () &&
					history ()->currentItem ().url ().isEmpty ())
				emit windowCloseRequested ();
		};

		switch (reply->error ())
		{
		case QNetworkReply::ProtocolUnknownError:
			if (XmlSettingsManager::Instance ()->
					property ("ExternalSchemes").toString ().split (' ')
					.contains (url.scheme ()))
				QDesktopServices::openUrl (url);
			else
				sendEnt ();
			break;
		case QNetworkReply::NoError:
		{
			auto found = FindFrame (url);
			if (!found)
			{
				if (XmlSettingsManager::Instance ()->
						property ("ParanoidDownloadsDetection").toBool () ||
						!mime.isEmpty ())
				{
					sendEnt ();
					break;
				}
				else
					qDebug () << Q_FUNC_INFO
							<< mime;
			}
			else
				qDebug () << Q_FUNC_INFO
					<< "but frame is found";
		}
		default:
		{
			int statusCode = reply->attribute (QNetworkRequest::HttpStatusCodeAttribute).toInt ();

			qDebug () << Q_FUNC_INFO
					<< "general unsupported content"
					<< url
					<< reply->error ()
					<< reply->errorString ();

			const auto& data = MakeErrorReplyContents (statusCode,
					url, reply->errorString (), QtNetwork);

			if (auto found = FindFrame (url))
				found->setHtml (data, url);
			else if (LoadingURL_ == url)
				mainFrame ()->setHtml (data, url);
			break;
		}
		}
	}

	void CustomWebPage::FillErrorSuggestions ()
	{
		QString checkDown = tr ("<a href=\"http://downforeveryoneorjustme.com/{host}\" "
				"target=\"_blank\">check</a> if the site <strong>{host}</strong> is down for you only;",
				"{host} would be substituded with site's host name.");
		QString tryAgainLater = tr ("try again later");
		QString contactRemoteAdmin = tr ("contact remote server's administrator "
				"(typically at <a href=\"mailto:webmaster@{host}\">webmaster@{host}</a>)");
		QString contactSystemAdmin = tr ("contact your system/network administrator, "
				"especially if you can't load any single page");
		QString checkProxySettings = tr ("check your proxy settings");

		Error2Suggestions_ [QtNetwork] [QNetworkReply::ConnectionRefusedError]
				<< tryAgainLater + ";"
				<< contactRemoteAdmin + ";"
				<< checkDown;
		Error2Suggestions_ [QtNetwork] [QNetworkReply::RemoteHostClosedError]
				<< tryAgainLater + ";"
				<< contactRemoteAdmin + ";"
				<< checkDown;
		Error2Suggestions_ [QtNetwork] [QNetworkReply::HostNotFoundError]
				<< tr ("check if the URL is written correctly;")
				<< tr ("try changing your DNS servers;")
				<< tr ("make sure that LeechCraft is allowed to access the Internet and particularly web sites;")
				<< contactSystemAdmin + ";"
				<< checkDown;
		Error2Suggestions_ [QtNetwork] [QNetworkReply::TimeoutError]
				<< tryAgainLater + ";"
				<< tr ("check whether some downloads consume too much bandwidth: try limiting their speed or reducing number of connections for them;")
				<< contactSystemAdmin + ";"
				<< contactRemoteAdmin + ";"
				<< checkDown;
		Error2Suggestions_ [QtNetwork] [QNetworkReply::OperationCanceledError]
				<< tr ("try again.");
		Error2Suggestions_ [QtNetwork] [QNetworkReply::SslHandshakeFailedError]
				<< tr ("make sure that remote server is really what it claims to be;")
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::TemporaryNetworkFailureError]
				<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyConnectionRefusedError]
				<< tryAgainLater + ";"
				<< checkProxySettings + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyConnectionClosedError]
				<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyNotFoundError] =
				Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyConnectionRefusedError];
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyTimeoutError] =
				Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyConnectionRefusedError];
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyAuthenticationRequiredError] =
				Error2Suggestions_ [QtNetwork] [QNetworkReply::ProxyConnectionRefusedError];
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ContentNotFoundError]
				<< tr ("check if the URL is written correctly;")
				<< tr ("go to web site's <a href=\"{schema}://{host}/\">main page</a> and find the required page from there.");
		Error2Suggestions_ [QtNetwork] [QNetworkReply::AuthenticationRequiredError]
				<< tr ("check the login and password you entered and try again");
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ContentReSendError]
				<< tryAgainLater + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProtocolUnknownError]
				<< tr ("check if the URL is written correctly, particularly, the part before the '://';")
				<< tr ("try installing plugins that are known to support this protocol;")
				<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProtocolInvalidOperationError]
										<< tryAgainLater + ";"
				<< contactRemoteAdmin + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::UnknownNetworkError]
										<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::UnknownProxyError]
				<< checkProxySettings + ";"
				<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::UnknownContentError]
				<< tryAgainLater + ";"
				<< contactSystemAdmin + ".";
		Error2Suggestions_ [QtNetwork] [QNetworkReply::ProtocolFailure]
				<< tryAgainLater + ";"
				<< contactRemoteAdmin + ";"
				<< contactSystemAdmin + ".";

		Error2Suggestions_ [Http] [404] = Error2Suggestions_ [QtNetwork] [QNetworkReply::ContentNotFoundError];
	}

	QString CustomWebPage::MakeErrorReplyContents (int statusCode,
			const QUrl& url, const QString& errorString, ErrorDomain domain) const
	{
		QFile file (":/resources/html/generalerror.html");
		file.open (QIODevice::ReadOnly);
		QString data = file.readAll ();
		data.replace ("{title}",
				tr ("Error loading %1")
					.arg (url.toString ()));
		if (statusCode &&
				domain == Http)
			data.replace ("{subtitle}",
					tr ("%1 (%2)")
						.arg (errorString)
						.arg (statusCode));
		else
			data.replace ("{subtitle}",
					tr ("%1")
						.arg (errorString));
		QString bodyContents = tr ("The page you tried to access cannot be loaded now.");

		QStringList suggestions = Error2Suggestions_ [domain] [statusCode];
		QString additionalContents;
		if (suggestions.size ())
		{
			bodyContents += "<br />";
			bodyContents += tr ("Try doing the following:");

			additionalContents += "<ul class=\"suggestionslist\"><li class=\"suggestionitem\">";
			additionalContents += suggestions.join ("</li><li class=\"suggestionitem\">");
			additionalContents += "</li></ul>";
		}
		data.replace ("{body}", bodyContents);
		data.replace ("{additional}", additionalContents);

		if (data.contains ("{host}"))
			data.replace ("{host}", url.host ());
		if (data.contains ("{schema}"))
			data.replace ("{schema}", url.scheme ());

		QBuffer ib;
		ib.open (QIODevice::ReadWrite);
		const auto& px = Core::Instance ().GetProxy ()->GetIconThemeManager ()->
				GetIcon ("dialog-error").pixmap (32, 32);
		px.save (&ib, "PNG");

		data.replace ("{img}",
				QByteArray ("data:image/png;base64,") + ib.buffer ().toBase64 ());
		return data;
	}

	void CustomWebPage::handleWindowCloseRequested ()
	{
		emit hookWindowCloseRequested (IHookProxy_ptr (new Util::DefaultHookProxy),
				this);
	}

	bool CustomWebPage::acceptNavigationRequest (QWebFrame *frame,
			const QNetworkRequest& other, QWebPage::NavigationType type)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QNetworkRequest request = other;
		emit hookAcceptNavigationRequest (proxy, this, frame, request, type);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toBool ();

		proxy->FillValue ("request", request);

		QString scheme = request.url ().scheme ();
		if (scheme == "mailto" ||
				scheme == "ftp")
		{
			const auto& e = Util::MakeEntity (request.url (),
					QString (),
					FromUserInitiated);
			auto em = Core::Instance ().GetProxy ()->GetEntityManager ();
			if (em->CouldHandle (e))
				em->HandleEntity (e);
			else
				QDesktopServices::openUrl (request.url ());
			return false;
		}

		if (frame)
			HandleForms (frame, request, type);

		if (type == NavigationTypeLinkClicked &&
				(MouseButtons_ == Qt::MidButton ||
					Modifiers_ & Qt::ControlModifier))
		{
			bool invert = Modifiers_ & Qt::ShiftModifier;

			CustomWebView *view = Core::Instance ().MakeWebView (invert);
			view->Load (request);

			MouseButtons_ = Qt::NoButton;
			Modifiers_ = Qt::NoModifier;
			return false;
		}

		if (frame == mainFrame ())
			LoadingURL_ = request.url ();

		return QWebPage::acceptNavigationRequest (frame, request, type);
	}

	QString CustomWebPage::chooseFile (QWebFrame *frame, const QString& thsuggested)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString suggested = thsuggested;
		emit hookChooseFile (proxy, this, frame, suggested);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toString ();

		proxy->FillValue ("suggested", suggested);

		return QWebPage::chooseFile (frame, suggested);
	}

	QObject* CustomWebPage::createPlugin (const QString& thclsid, const QUrl& thurl,
			const QStringList& thnames, const QStringList& thvalues)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString clsid = thclsid;
		QUrl url = thurl;
		QStringList names = thnames;
		QStringList values = thvalues;
		emit hookCreatePlugin (proxy, this,
				clsid, url, names, values);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().value<QObject*> ();

		proxy->FillValue ("clsid", clsid);
		proxy->FillValue ("url", url);
		proxy->FillValue ("names", names);
		proxy->FillValue ("values", values);

		return QWebPage::createPlugin (clsid, url, names, values);
	}

	QWebPage* CustomWebPage::createWindow (QWebPage::WebWindowType type)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookCreateWindow (proxy, this, type);
		if (proxy->IsCancelled ())
			return qobject_cast<QWebPage*> (proxy->GetReturnValue ().value<QObject*> ());

		switch (type)
		{
		case QWebPage::WebBrowserWindow:
			return Core::Instance ().NewURL (QUrl ())->GetView ()->page ();
		case QWebPage::WebModalDialog:
		{
			BrowserWidget *widget = new BrowserWidget (view ());
			widget->FinalizeInit ();
			widget->setWindowFlags (Qt::Dialog);
			widget->setAttribute (Qt::WA_DeleteOnClose);
			widget->setWindowModality (Qt::ApplicationModal);
			connect (widget,
					SIGNAL (gotEntity (const LeechCraft::Entity&)),
					&Core::Instance (),
					SIGNAL (gotEntity (const LeechCraft::Entity&)));
			connect (widget,
					SIGNAL (titleChanged (const QString&)),
					widget,
					SLOT (setWindowTitle (const QString&)));
			widget->show ();
			return widget->GetView ()->page ();
		}
		default:
			qWarning () << Q_FUNC_INFO
					<< "unknown type"
					<< type;
			return 0;
		}
	}

	void CustomWebPage::javaScriptAlert (QWebFrame *frame, const QString& thmsg)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString msg = thmsg;
		emit hookJavaScriptAlert (proxy,
				this, frame, msg);
		if (proxy->IsCancelled ())
			return;

		proxy->FillValue ("message", msg);

		QWebPage::javaScriptAlert (frame, msg);
	}

	bool CustomWebPage::javaScriptConfirm (QWebFrame *frame, const QString& thmsg)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString msg = thmsg;
		emit hookJavaScriptConfirm (proxy,
				this, frame, msg);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toBool ();

		proxy->FillValue ("message", msg);

		return QWebPage::javaScriptConfirm (frame, msg);
	}

	void CustomWebPage::javaScriptConsoleMessage (const QString& thmsg, int line,
			const QString& thsid)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString msg = thmsg;
		QString sid = thsid;
		emit hookJavaScriptConsoleMessage (proxy,
				this, msg, line, sid);
		if (proxy->IsCancelled ())
			return;

		proxy->FillValue ("message", msg);
		proxy->FillValue ("line", line);
		proxy->FillValue ("sourceID", sid);

		QWebPage::javaScriptConsoleMessage (msg, line, sid);
	}

	bool CustomWebPage::javaScriptPrompt (QWebFrame *frame, const QString& thpr,
			const QString& thdef, QString *result)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		QString pr = thpr;
		QString def = thdef;
		emit hookJavaScriptPrompt (proxy,
				this, frame, pr, def, *result);
		proxy->FillValue ("result", *result);
		if (proxy->IsCancelled ())
			return proxy->GetReturnValue ().toBool ();

		proxy->FillValue ("message", pr);
		proxy->FillValue ("default", def);

		return QWebPage::javaScriptPrompt (frame, pr, def, result);
	}

	QString CustomWebPage::userAgentForUrl (const QUrl& url) const
	{
		QString ua = Core::Instance ().GetUserAgent (url, this);
		if (ua.isEmpty ())
			return QWebPage::userAgentForUrl (url);
		else
			return ua;
	}

	QWebFrame* CustomWebPage::FindFrame (const QUrl& url)
	{
		QList<QWebFrame*> frames;
		frames.append (mainFrame ());
		while (!frames.isEmpty ())
		{
			QWebFrame *frame = frames.takeFirst ();
			if (frame->url () == url)
				return frame;
			frames << frame->childFrames ();
		}
		return 0;
	}

	namespace
	{
		bool CheckData (const PageFormsData_t& data,
				QWebFrame *frame,
				const QNetworkRequest& request = QNetworkRequest ())
		{
			if (data.isEmpty ())
			{
				qWarning () << Q_FUNC_INFO
					<< "no form data for"
					<< frame
					<< request.url ();
				return false;
			}
			return true;
		}

		QPair<PageFormsData_t, QMap<ElementData, QWebElement>> HarvestForms (QWebFrame *frame, const QUrl& reqUrl = QUrl ())
		{
			PageFormsData_t formsData;
			QMap<ElementData, QWebElement> ed2element;

			QUrl pageUrl = frame->url ();
			QWebElementCollection forms = frame->findAllElements ("form");
			Q_FOREACH (const QWebElement& form, forms)
			{
				QUrl relUrl = QUrl::fromEncoded (form.attribute ("action").toUtf8 ());
				QUrl actionUrl = pageUrl.resolved (relUrl);
				if (reqUrl.isValid () && actionUrl != reqUrl)
					continue;

				QString url = actionUrl.toEncoded ();
				QString formId = QString ("%1<>%2<>%3")
						.arg (url)
						.arg (form.attribute ("id"))
						.arg (form.attribute ("name"));

				QWebElementCollection children = form.findAll ("input");
				Q_FOREACH (QWebElement child, children)
				{
					QString elemType = child.attribute ("type");
					if (elemType == "hidden" ||
							elemType == "submit")
						continue;

					QString name = child.attribute ("name");
					// Ugly workaround for https://bugs.webkit.org/show_bug.cgi?id=32865
					QString value = child.evaluateJavaScript ("this.value").toString ();

					if (name.isEmpty () ||
							(reqUrl.isValid () && value.isEmpty ()))
						continue;

					ElementData ed =
					{
						pageUrl,
						formId,
						name,
						elemType,
						value
					};

					formsData [name] << ed;
					ed2element [ed] = child;
				}
			}

			return qMakePair (formsData, ed2element);
		}
	};

	void CustomWebPage::HandleForms (QWebFrame *frame,
			const QNetworkRequest& request, QWebPage::NavigationType type)
	{
		if (type != NavigationTypeFormSubmitted)
			return;

		QUrl pageUrl = frame->url ();
		// Check if this should be emitted at all
		if (Core::Instance ().GetStorageBackend ()->
				GetFormsIgnored (pageUrl.toString ()))
			return;

		PageFormsData_t formsData =
				HarvestForms (frame ? frame : mainFrame (),
						request.url ()).first;

		if (!CheckData (formsData, frame, request))
			return;

		if (FilledState_ == formsData)
			return;

		emit storeFormData (formsData);
	}

	namespace
	{
		ElementsData_t::const_iterator FindElement (const ElementData& filled,
				const ElementsData_t& list, bool strict)
		{
			auto typeChecker = [filled] (const ElementData& ed)
				{ return ed.Type_ == filled.Type_; };
			auto urlChecker = [filled] (const ElementData& ed)
				{ return ed.PageURL_ == filled.PageURL_; };
			auto formIdChecker = [filled] (const ElementData& ed)
				{ return ed.FormID_ == filled.FormID_; };

			auto pos = std::find_if (list.begin (), list.end (),
					[typeChecker, urlChecker, formIdChecker] (const ElementData& ed)
						{ return typeChecker (ed) && urlChecker (ed) && formIdChecker (ed); });
			if (!strict)
			{
				if (pos == list.end ())
					pos = std::find_if (list.begin (), list.end (),
							[typeChecker, formIdChecker] (const ElementData& ed)
								{ return typeChecker (ed) && formIdChecker (ed); });
				if (pos == list.end ())
					pos = std::find_if (list.begin (), list.end (),
							[typeChecker, urlChecker] (const ElementData& ed)
								{ return typeChecker (ed) && urlChecker (ed); });
			}

			return pos;
		}
	}

	void CustomWebPage::fillForms (QWebFrame *frame)
	{
		PageFormsData_t formsData;

		auto pair = HarvestForms (frame);

		if (pair.first.isEmpty ())
		{
			FilledState_.clear ();
			return;
		}

		QVariantList values;
		QList<QByteArray> keys;
		const auto& pairFirstKeys = pair.first.keys ();
		for (const auto& name : pairFirstKeys)
		{
			const auto& key = "org.LeechCraft.Poshuku.Forms.InputByName/" + name.toUtf8 ();
			keys << key;
			values.append (Util::GetPersistentData (key, Core::Instance ().GetProxy ()));
		}

		const int size = keys.size ();
		if (values.size () != size)
			return;

		for (int i = 0; i < size; ++i)
		{
			QString inputName = QString::fromUtf8 (keys.at (i));
			if (inputName.isEmpty ())
			{
				qWarning () << Q_FUNC_INFO
						<< "empty input.name for"
						<< keys.at (i);
				continue;
			}

			const QVariantList vars = values.at (i).toList ();
			if (!vars.size ())
				continue;

			const ElementsData_t list = pair.first [pairFirstKeys.at (i)];
			ElementsData_t::const_iterator source = list.end ();
			QString value;
			Q_FOREACH (const QVariant& var, vars)
			{
				const ElementData ed = var.value<ElementData> ();
				source = FindElement (ed, list, true);
				if (source < list.end ())
				{
					value = ed.Value_;
					break;
				}
			}
			if (source >= list.end ())
				Q_FOREACH (const QVariant& var, vars)
				{
					const ElementData ed = var.value<ElementData> ();
					source = FindElement (ed, list, false);
					if (source < list.end ())
					{
						value = ed.Value_;
						break;
					}
				}

			if (source < list.end ())
				pair.second [*source].setAttribute ("value", value);
		}

		Q_FOREACH (QWebFrame *childFrame, frame->childFrames ())
			fillForms (childFrame);

		FilledState_ = HarvestForms (frame ? frame : mainFrame ()).first;
	}
}
}
