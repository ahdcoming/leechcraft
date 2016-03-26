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

#include "greasemonkey.h"
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QTextStream>
#include <QtDebug>

namespace LeechCraft
{
namespace Poshuku
{
namespace FatApe
{
	GreaseMonkey::GreaseMonkey (QWebFrame *frame, IProxyObject *proxy, const UserScript& script)
	: Frame_ (frame)
	, Proxy_ (proxy)
	, Script_ (script)
	{
	}

	std::shared_ptr<QSettings> GreaseMonkey::GetStorage () const
	{
		const auto& orgName = QCoreApplication::organizationName ();
		const auto& sName = QCoreApplication::applicationName () + "_Poshuku_FatApe_Storage";
		std::shared_ptr<QSettings> settings { new QSettings { orgName, sName },
			[] (QSettings *settings)
			{
					settings->endGroup ();
					settings->endGroup ();
					delete settings;
			} };
		settings->beginGroup (QString::number (qHash (Script_.Namespace ())));
		settings->beginGroup (QString::number (qHash (Script_.Name ())));
		return settings;
	}

	void GreaseMonkey::addStyle (QString css)
	{
		QString js;
		js += "var el = document.createElement('style');";
		js += "el.type = 'text/css';";
		js += "el.innerHTML = '* { " + css.replace ('\'', "\\'") + " }'";
		js += "document.body.insertBefore(el, document.body.firstChild);";
		Frame_->evaluateJavaScript (js);
	}

	void GreaseMonkey::deleteValue (const QString& name)
	{
		qDebug () << Q_FUNC_INFO << name;
		GetStorage ()->remove (name);
	}

	QVariant GreaseMonkey::getValue (const QString& name)
	{
		qDebug () << Q_FUNC_INFO << name;
		return getValue (name, QVariant ());
	}

	QVariant GreaseMonkey::getValue (const QString& name, QVariant defVal)
	{
		qDebug () << Q_FUNC_INFO << name << "with" << defVal;
		return GetStorage ()->value (name, defVal);
	}

	QVariant GreaseMonkey::listValues ()
	{
		return GetStorage ()->allKeys ();
	}

	void GreaseMonkey::setValue (const QString& name, QVariant value)
	{
		qDebug () << Q_FUNC_INFO << name << "to" << value;
		GetStorage ()->setValue (name, value);
	}

	void GreaseMonkey::openInTab (const QString& url)
	{
		if (Proxy_)
			Proxy_->OpenInNewTab (url);
	}

	QString GreaseMonkey::getResourceText (const QString& resourceName)
	{
		QFile resource (Script_.GetResourcePath (resourceName));

		return resource.open (QFile::ReadOnly) ?
			QTextStream (&resource).readAll () :
			QString ();
	}

	QString GreaseMonkey::getResourceURL (const QString& resourceName)
	{
		QSettings settings (QCoreApplication::organizationName (),
			QCoreApplication::applicationName () + "_Poshuku_FatApe");
		QString mimeType = settings.value (QString ("resources/%1/%2/%3")
				.arg (qHash (Script_.Namespace ()))
				.arg (Script_.Name ())
				.arg (resourceName)).toString ();
		QFile resource (Script_.GetResourcePath (resourceName));

		return resource.open (QFile::ReadOnly) ?
			QString ("data:%1;base64,%2")
				.arg (mimeType)
				.arg (QString (resource.readAll ().toBase64 ())
						//The result is a base64 encoded URI, which is then also URI encoded,
						//as suggested by Wikipedia(http://en.wikipedia.org/wiki/Base64#URL_applications),
						//because of "+" and "/" characters in the base64 alphabet.
						//http://wiki.greasespot.net/GM_getResourceURL#Returns
						.replace ("+", "%2B")
						.replace ("/", "%2F")) :
			QString ();
	}
}
}
}
