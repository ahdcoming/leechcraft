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
#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include "xsdconfig.h"

class QWidget;
class QStackedWidget;
class QListWidget;
class QPushButton;
class QDomElement;
class QGridLayout;
class QDomDocument;
class QAbstractItemModel;

namespace LeechCraft
{
class ItemHandlerFactory;

namespace Util
{
	class BaseSettingsManager;

	class XmlSettingsDialog : public QObject
	{
		Q_OBJECT

		QWidget * const Widget_;

		QStackedWidget * const Pages_;

		QStringList Titles_;
		QList<QStringList> IconNames_;

		BaseSettingsManager *WorkingObject_ = nullptr;
		QString DefaultLang_ = "en";
		std::shared_ptr<QDomDocument> Document_;
		QList<QWidget*> Customs_;
		ItemHandlerFactory * const HandlersManager_;

		QString Basename_;
		QString TrContext_;
	public:
		struct LangElements
		{
			bool Valid_;
			QPair<bool, QString> Label_;
			QPair<bool, QString> Suffix_;
			QPair<bool, QString> SpecialValue_;
		};

		XMLSETTINGSMANAGER_API XmlSettingsDialog ();
		XMLSETTINGSMANAGER_API virtual ~XmlSettingsDialog ();

		XMLSETTINGSMANAGER_API void RegisterObject (BaseSettingsManager*, const QString&);
		XMLSETTINGSMANAGER_API BaseSettingsManager* GetManagerObject () const;

		XMLSETTINGSMANAGER_API QWidget* GetWidget () const;

		/** @brief Returns the current XML.
		 *
		 * Returns the XML with the default settings set to current
		 * settings.
		 *
		 * @return String with the current XML.
		 */
		XMLSETTINGSMANAGER_API QString GetXml () const;

		/** @brief Sets the settings to XML's ones.
		 *
		 * Sets settings to defaults of the passed XML.
		 *
		 * @param[in] xml The XML to take data from.
		 */
		XMLSETTINGSMANAGER_API void MergeXml (const QByteArray& xml);

		XMLSETTINGSMANAGER_API QList<int> HighlightMatches (const QString& query);

		/** @brief Sets custom widget mentioned in the XML.
		 *
		 * Sets the placeholder named name, mentioned in the XML, to
		 * contain the widget. If there is no name or more than one
		 * name, throws std::runtime_error. Widget's slots accept()
		 * and reject() would be called when the dialog is accepted
		 * or rejected.
		 *
		 * Default layout of the placeholder is QVBoxLayout, so if
		 * you call this function more than once with the same name
		 * but different widget parameters, it would create a nice
		 * vertical layout of all added widgets in the placeholder.
		 *
		 * It's your duty to do anything related to the widget,
		 * XmlSettingsDialog would only make some place for it and
		 * notify if the dialog is accepted or rejected.
		 *
		 * @param[in] name Name of the placeholder to replace.
		 * @param[in] widget The widget to add to the layout of the
		 * placeholder.
		 *
		 * @exception std::runtime_error If there is no or more than
		 * one name.
		 */
		XMLSETTINGSMANAGER_API void SetCustomWidget (const QString& name,
				QWidget *widget);

		/** @brief Sets the source model for the given property.
		 *
		 * Sets the datasource for the given item with the given
		 * property name to be the given source model.
		 *
		 * The source model's parent() is used to manipulate the item
		 * data. There should be two public slots in the parent:
		 * - addRequested(QString, QVariantList)
		 * - removeRequested(QString, QModelIndexList)
		 *
		 * These functions are called by the XmlSettingsDialog when
		 * user chooses to add or remove a row correspondingly.
		 *
		 * The first parameter in both functions is set to the
		 * property name. The second parameter in the addRequested()
		 * function is set to the list of data values for the row.
		 * The model is expected to contain the information about
		 * what data types are expected to be in what row. For that,
		 * each horizontal header item should contain an integer for
		 * the DataSources::DataSourceRole::FieldType role. The
		 * integers should correspond to values of the
		 * DataSources::DataFieldType enumeration.
		 *
		 * @param[in] property The identifier of the property.
		 * @param[in] source The new datasource.
		 */
		XMLSETTINGSMANAGER_API void SetDataSource (const QString& property,
				QAbstractItemModel *source);

		/** @brief Sets the current page to page.
		 *
		 * @param[in] page Number of the page.
		 */
		XMLSETTINGSMANAGER_API void SetPage (int page);

		/** @brief Returns the list of all the pages.
		 *
		 * @return The names of the pages.
		 */
		XMLSETTINGSMANAGER_API QStringList GetPages () const;

		/** @brief Returns the icon associated with the given page.
		 */
		XMLSETTINGSMANAGER_API QIcon GetPageIcon (int page) const;

		/** @brief Returns the human-readable label for the given
		 * element.
		 *
		 * Returns the proper label for an element according to the
		 * current locale and XML settings.
		 *
		 * @param[in] element The element for which label should be
		 * returned.
		 * @return The label suitable for showing to the user in the
		 * current language.
		 *
		 * @sa GetLangElements()
		 */
		QString GetLabel (const QDomElement& element) const;

		QString GetDescription (const QDomElement& element) const;

		void SetTooltip (QWidget *widget, const QDomElement& element) const;

		/** @brief Returns the current value for the given element.
		 *
		 * This function checks the object associated with this
		 * settings dialog and returns a value previously stored or
		 * default value if no previously set value exists.
		 *
		 * If ignoreObject is set, this function ignores the
		 * preferences already present and just parses the XML file.
		 * In this case, the default value is returned.
		 *
		 * @param[in] element The element for which the preferences
		 * value should be returned.
		 * @param[in] ignoreObject Whether associated object should
		 * be ignored.
		 * @return The current preferences value for the element.
		 */
		QVariant GetValue (const QDomElement& element,
				bool ignoreObject = false) const;

		/** @brief Returns the list of images associated with the
		 * given element.
		 *
		 * This function iterates over all children with name
		 * "binary" and creates the list of images that could be
		 * retreieved from those binary children.
		 *
		 * @param[in] element The element to collect images from.
		 * @return The list of images, if any.
		 */
		QList<QImage> GetImages (const QDomElement& element) const;

		/** @brief Parses the given element under the given parent
		 * widget.
		 *
		 * @param[in] element The element to inspect.
		 * @param[in] parent The parent widget under which to build
		 * up the representation.
		 */
		void ParseEntity (const QDomElement& element, QWidget *parent);

		/** @brief Get other human-readable messages from the
		 * element.
		 *
		 * This function is similar to GetLabel(), but instead it
		 * returns a slightly different list of user-visible
		 * elements.
		 *
		 * @param[in] element The element to inspect.
		 * @return The LangElements structure filled with available
		 * language/locale-dependent strings.
		 *
		 * @sa GetLabel()
		 */
		LangElements GetLangElements (const QDomElement& element) const;

		/** @brief Get XML base name of this XML settings dialog.
		 */
		QString GetBasename () const;
	private:
		void HandleDeclaration (const QDomElement&);
		void ParsePage (const QDomElement&);
		void ParseItem (const QDomElement&, QWidget*);
		void UpdateXml (bool = false);
		void UpdateSingle (const QString&, const QVariant&, QDomElement&);
		void SetValue (QWidget*, const QVariant&);
	protected:
		bool eventFilter (QObject*, QEvent*);
	public Q_SLOTS:
		virtual void accept ();
		virtual void reject ();
	private Q_SLOTS:
		void handleCustomDestroyed ();
		void handleMoreThisStuffRequested ();
		void handlePushButtonReleased ();
		void handleShowPageRequested (Util::BaseSettingsManager*, const QString&);
	Q_SIGNALS:
		XMLSETTINGSMANAGER_API void pushButtonClicked (const QString&);
		XMLSETTINGSMANAGER_API void moreThisStuffRequested (const QString&);
		XMLSETTINGSMANAGER_API void showPageRequested (Util::BaseSettingsManager*, const QString&);
	};

	typedef std::shared_ptr<XmlSettingsDialog> XmlSettingsDialog_ptr;
}
}
