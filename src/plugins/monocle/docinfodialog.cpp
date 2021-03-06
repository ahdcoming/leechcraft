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

#include "docinfodialog.h"
#include <QStandardItemModel>
#include <QtDebug>
#include <util/sll/slotclosure.h>
#include "interfaces/monocle/idocument.h"
#include "interfaces/monocle/ihavefontinfo.h"

namespace LeechCraft
{
namespace Monocle
{
	DocInfoDialog::DocInfoDialog (const QString& filepath, const IDocument_ptr& doc, QWidget *parent)
	: QDialog { parent }
	, FontsModel_ { new QStandardItemModel { this } }
	{
		Ui_.setupUi (this);

		Ui_.FontsView_->setModel (FontsModel_);

		Ui_.FilePath_->setText (filepath);

		const auto& info = doc->GetDocumentInfo ();
		Ui_.Title_->setText (info.Title_);
		Ui_.Subject_->setText (info.Subject_);
		Ui_.Author_->setText (info.Author_);
		Ui_.Genres_->setText (info.Genres_.join ("; "));
		Ui_.Keywords_->setText (info.Keywords_.join ("; "));
		Ui_.Date_->setText (info.Date_.toString ());

		const auto ihf = qobject_cast<IHaveFontInfo*> (doc->GetQObject ());
		Ui_.TabWidget_->setTabEnabled (Ui_.TabWidget_->indexOf (Ui_.FontsTab_), ihf);

		if (ihf)
		{
			const auto pending = ihf->RequestFontInfos ();
			new Util::SlotClosure<Util::DeleteLaterPolicy>
			{
				[this, pending] { HandleFontsInfo (pending->GetFontInfos ()); },
				pending->GetQObject (),
				SIGNAL (ready ()),
				this
			};
		}
	}

	void DocInfoDialog::HandleFontsInfo (const QList<FontInfo>& infos)
	{
		FontsModel_->setHorizontalHeaderLabels ({ tr ("Name"), tr ("Path") });
		for (const auto& info : infos)
		{
			const QList<QStandardItem*> row
			{
				new QStandardItem { info.FontName_ },
				new QStandardItem { info.IsEmbedded_ ?
						tr ("embedded") :
						info.LocalPath_ }
			};
			for (auto item : row)
				item->setEditable (false);
			FontsModel_->appendRow (row);
		}
	}
}
}
