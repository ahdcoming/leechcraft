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
#include <QStringList>
#include <QDateTime>
#include <QMetaType>
#include <QHash>
#include <util/db/oraltypes.h>

namespace LeechCraft
{
namespace Poleemery
{
	enum class AccType
	{
		Cash,
		BankAccount
	};

	QString ToHumanReadable (AccType);

	struct Account
	{
		Util::oral::PKey<int> ID_;
		AccType Type_;
		QString Name_;
		QString Currency_;

		static QString ClassName () { return "Account"; }

		static QString FieldNameMorpher (const QString& str) { return str; }
	};

	bool operator== (const Account&, const Account&);
	bool operator!= (const Account&, const Account&);
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::Account,
		ID_,
		Type_,
		Name_,
		Currency_)

Q_DECLARE_METATYPE (LeechCraft::Poleemery::Account)

namespace LeechCraft
{
namespace Poleemery
{
	enum class EntryType
	{
		Expense,
		Receipt
	};

	struct EntryBase
	{
		Util::oral::PKey<int> ID_ = -1;
		Util::oral::References<&Account::ID_> AccountID_ = -1;

		/** This actually price.
		 */
		double Amount_ = 0;
		QString Name_;
		QString Description_;
		QDateTime Date_;

		EntryBase () = default;
		EntryBase (int accId, double amount, const QString& name, const QString& descr, const QDateTime& dt);
		virtual ~EntryBase () = default;

		virtual EntryType GetType () const = 0;
	};

	typedef std::shared_ptr<EntryBase> EntryBase_ptr;

	struct NakedExpenseEntry : EntryBase
	{
		double Count_ = 0;
		QString Shop_;

		QString EntryCurrency_;
		double Rate_ = 0;

		static QString ClassName () { return "NakedExpenseEntry"; }

		static QString FieldNameMorpher (const QString& str) { return str; }

		EntryType GetType () const { return EntryType::Expense; }

		NakedExpenseEntry () = default;
		NakedExpenseEntry (int accId, double amount,
				const QString& name, const QString& descr, const QDateTime& dt,
				double count, const QString& shop, const QString& currency, double rate);
	};

	struct ExpenseEntry : NakedExpenseEntry
	{
		QStringList Categories_;

		ExpenseEntry () = default;
		ExpenseEntry (const NakedExpenseEntry&);
		ExpenseEntry (int accId, double amount,
				const QString& name, const QString& descr, const QDateTime& dt,
				double count, const QString& shop, const QString& currency, double rate,
				const QStringList& cats);
	};

	typedef std::shared_ptr<ExpenseEntry> ExpenseEntry_ptr;
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::NakedExpenseEntry,
		ID_,
		AccountID_,
		Amount_,
		Name_,
		Description_,
		Date_,
		Count_,
		Shop_,
		EntryCurrency_,
		Rate_)

namespace LeechCraft
{
namespace Poleemery
{
	struct Category
	{
		Util::oral::PKey<int> ID_ = -1;
		Util::oral::Unique<QString> Name_;

		Category () = default;
		explicit Category (const QString&);

		static QString ClassName () { return "Category"; }

		static QString FieldNameMorpher (const QString& str) { return str; }
	};
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::Category,
		ID_,
		Name_)

namespace LeechCraft
{
namespace Poleemery
{
	struct CategoryLink
	{
		Util::oral::PKey<int> ID_ = -1;
		Util::oral::References<&Category::ID_> Category_;
		Util::oral::References<&NakedExpenseEntry::ID_> Entry_;

		CategoryLink () = default;
		CategoryLink (const Category&, const NakedExpenseEntry&);

		static QString ClassName () { return "CategoryLink"; }

		static QString FieldNameMorpher (const QString& str) { return str; }
	};
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::CategoryLink,
		ID_,
		Category_,
		Entry_)

namespace LeechCraft
{
namespace Poleemery
{
	struct ReceiptEntry : EntryBase
	{
		using EntryBase::EntryBase;

		static QString ClassName () { return "ReceiptEntry"; }

		static QString FieldNameMorpher (const QString& str) { return str; }

		EntryType GetType () const { return EntryType::Receipt; }
	};
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::ReceiptEntry,
		ID_,
		AccountID_,
		Amount_,
		Name_,
		Description_,
		Date_)

namespace LeechCraft
{
namespace Poleemery
{
	struct Rate
	{
		Util::oral::PKey<int> ID_;

		QString Code_;
		QDateTime SnapshotTime_;
		double Rate_;

		static QString ClassName () { return "Rate"; }

		static QString FieldNameMorpher (const QString& str) { return str; }
	};
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Poleemery::Rate,
		ID_,
		Code_,
		SnapshotTime_,
		Rate_)

namespace LeechCraft
{
namespace Poleemery
{
	struct BalanceInfo
	{
		double Total_;
		QHash<int, double> Accs_;
	};

	struct EntryWithBalance
	{
		EntryBase_ptr Entry_;
		BalanceInfo Balance_;
	};
}
}
